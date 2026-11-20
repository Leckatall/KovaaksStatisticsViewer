#include "benchmark_store.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace ksv::qt_data {
    namespace {
        using namespace data;
        using namespace ksv::domain;

        constexpr int kSchemaVersion = 2;

        bool exactKeys(const QJsonObject &object, const std::initializer_list<const char *> keys) {
            if (object.size() != static_cast<qsizetype>(keys.size())) return false;
            return std::ranges::all_of(keys, [&](const auto &key) { return object.contains(key); });
        }

        std::optional<std::string> nonEmptyIdString(const QJsonValue &value) {
            if (!value.isString()) return std::nullopt;
            const auto text = value.toString().toStdString();
            if (text.empty()) return std::nullopt;
            return text;
        }

        std::optional<BenchmarkColor> decodeColor(const QJsonValue &value) {
            if (!value.isArray()) return std::nullopt;
            const auto array = value.toArray();
            if (array.size() != 4) return std::nullopt;
            BenchmarkColor color{};
            uint8_t *channels[4] = {&color.red, &color.green, &color.blue, &color.alpha};
            for (int i = 0; i < 4; ++i) {
                if (!array[i].isDouble()) return std::nullopt;
                const auto number = array[i].toDouble();
                if (std::floor(number) != number || number < 0 || number > 255) return std::nullopt;
                *channels[i] = static_cast<uint8_t>(number);
            }
            return color;
        }

        QJsonArray encodeColor(const BenchmarkColor &c) {
            return QJsonArray{c.red, c.green, c.blue, c.alpha};
        }

        std::optional<Threshold> decodeThreshold(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"tierId", "score"}) || !object["score"].isDouble()) return std::nullopt;
            const auto tierId = nonEmptyIdString(object["tierId"]);
            if (!tierId) return std::nullopt;
            return Threshold{TierId{*tierId}, object["score"].toDouble()};
        }

        std::optional<BenchmarkScenario> decodeEntry(const QJsonValue &value, int version) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!(version == 1 ? exactKeys(object, {"id", "name", "hash", "thresholds"})
                              : exactKeys(object, {"id", "name", "hashes", "thresholds"})) ||
                !object["name"].isString() ||
                !object["thresholds"].isArray())
                return std::nullopt;
            const auto id = nonEmptyIdString(object["id"]);
            if (!id) return std::nullopt;
            std::vector<std::string> hashes;
            if (version == 1) {
                if (!object["hash"].isNull()) {
                    if (!object["hash"].isString()) return std::nullopt;
                    const auto hash = object["hash"].toString().toStdString();
                    if (!hash.empty()) hashes.push_back(hash);
                }
            } else {
                if (!object["hashes"].isArray()) return std::nullopt;
                for (const auto &value: object["hashes"].toArray()) {
                    const auto hash = nonEmptyIdString(value);
                    if (!hash || std::ranges::find(hashes, *hash) != hashes.end()) return std::nullopt;
                    hashes.push_back(*hash);
                }
            }
            std::vector<Threshold> thresholds;
            for (const auto &item: object["thresholds"].toArray()) {
                const auto threshold = decodeThreshold(item);
                if (!threshold) return std::nullopt;
                thresholds.push_back(*threshold);
            }
            BenchmarkScenario entry{BenchmarkScenarioId{*id}, object["name"].toString().toStdString(),
                                std::move(hashes), std::move(thresholds)};
            return entry;
        }

        std::optional<std::vector<BenchmarkScenario>> decodeEntries(const QJsonValue &value, int version) {
            if (!value.isArray()) return std::nullopt;
            std::vector<BenchmarkScenario> entries;
            for (const auto &item: value.toArray()) {
                const auto entry = decodeEntry(item, version);
                if (!entry) return std::nullopt;
                entries.push_back(*entry);
            }
            return entries;
        }

        // The caller checks the object's exact key set, which differs between categories and subcategories.
        std::optional<BenchmarkGroup> decodeGroup(const QJsonObject &object, int version) {
            if (!object["name"].isString()) return std::nullopt;
            const auto id = nonEmptyIdString(object["id"]);
            const auto color = decodeColor(object["color"]);
            auto scenarios = decodeEntries(object["scenarios"], version);
            if (!id || !color || !scenarios) return std::nullopt;
            return BenchmarkGroup{GroupId{*id}, object["name"].toString().toStdString(), *color, std::move(*scenarios)};
        }

        std::optional<Category> decodeCategory(const QJsonValue &value, int version) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"id", "name", "color", "scenarios", "subcategories"}) ||
                !object["subcategories"].isArray())
                return std::nullopt;
            auto group = decodeGroup(object, version);
            if (!group) return std::nullopt;
            std::vector<Subcategory> subcategories;
            for (const auto &item: object["subcategories"].toArray()) {
                if (!item.isObject()) return std::nullopt;
                const auto sub = item.toObject();
                if (!exactKeys(sub, {"id", "name", "color", "scenarios"})) return std::nullopt;
                auto subGroup = decodeGroup(sub, version);
                if (!subGroup) return std::nullopt;
                subcategories.push_back(Subcategory{std::move(*subGroup)});
            }
            return Category{std::move(*group), std::move(subcategories)};
        }

        std::optional<Tier> decodeTier(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"id", "name", "color"}) || !object["name"].isString()) return std::nullopt;
            const auto id = nonEmptyIdString(object["id"]);
            const auto color = decodeColor(object["color"]);
            if (!id || !color) return std::nullopt;
            return Tier{TierId{*id}, object["name"].toString().toStdString(), *color};
        }

        // Structural decode: shape + types only. Returns nullopt on any representation failure.
        std::optional<BenchmarkDefinition> decodeStructure(const QJsonObject &root, int version) {
            if (!exactKeys(root, {"schemaVersion", "id", "name", "tiers", "uncategorized", "categories"}) ||
                !root["name"].isString() || !root["tiers"].isArray() || !root["uncategorized"].isArray() ||
                !root["categories"].isArray())
                return std::nullopt;
            const auto id = nonEmptyIdString(root["id"]);
            if (!id) return std::nullopt;
            BenchmarkDefinition def;
            def.id = BenchmarkId{*id};
            def.name = root["name"].toString().toStdString();
            for (const auto &item: root["tiers"].toArray()) {
                const auto tier = decodeTier(item);
                if (!tier) return std::nullopt;
                def.tiers.push_back(*tier);
            }
            auto uncategorized = decodeEntries(root["uncategorized"], version);
            if (!uncategorized) return std::nullopt;
            def.uncategorized = std::move(*uncategorized);
            for (const auto &item: root["categories"].toArray()) {
                const auto category = decodeCategory(item, version);
                if (!category) return std::nullopt;
                def.categories.push_back(*category);
            }
            return def;
        }

        // Representation integrity that Invalidates a file: duplicate stable ids anywhere, and any
        // threshold referencing a tier the definition does not declare.
        bool hasStructuralIntegrity(const BenchmarkDefinition &def) {
            std::unordered_set<std::string> ids;
            const auto add = [&](const std::string &value) { return ids.insert(value).second; };
            if (!add(def.id.value)) return false;
            std::unordered_set<std::string> tierIds;
            for (const auto &tier: def.tiers) {
                if (!add(tier.id.value)) return false;
                tierIds.insert(tier.id.value);
            }
            for (const auto *group: def.groups())
                if (!add(group->id.value)) return false;
            for (const auto *entry: def.entries()) {
                if (!add(entry->id.value)) return false;
                for (const auto &threshold: entry->thresholds)
                    if (!tierIds.contains(threshold.tierId.value)) return false;  // dangling tier ref
            }
            return true;
        }

        std::variant<LoadedBenchmark, ProblemBenchmark> classifyBytes(const QByteArray &bytes) {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
                return ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}};
            const auto root = document.object();
            if (!root["schemaVersion"].isDouble())
                return ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}};
            const auto version = root["schemaVersion"].toDouble();
            if (std::floor(version) != version)
                return ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}};

            // Best-effort display fields for a problem entry, taken only if safely parseable.
            std::optional<std::string> displayName;
            if (root["name"].isString() && !root["name"].toString().isEmpty())
                displayName = root["name"].toString().toStdString();
            std::optional<BenchmarkId> embeddedId;
            if (const auto id = nonEmptyIdString(root["id"])) embeddedId = BenchmarkId{*id};

            if (static_cast<int>(version) > kSchemaVersion)
                return ProblemBenchmark{BenchmarkFileProblem::Unsupported, displayName, embeddedId,
                                        static_cast<int>(version)};
            if (version < 1)
                return ProblemBenchmark{BenchmarkFileProblem::Invalid, displayName, embeddedId, {}};

            const auto structure = decodeStructure(root, static_cast<int>(version));
            if (!structure || !hasStructuralIntegrity(*structure))
                return ProblemBenchmark{BenchmarkFileProblem::Invalid, displayName, embeddedId, {}};
            return LoadedBenchmark{*structure, validateBenchmark(*structure)};
        }

        QJsonObject encodeEntry(const BenchmarkScenario &entry) {
            QJsonArray thresholds;
            for (const auto &threshold: entry.thresholds)
                thresholds.append(QJsonObject{{"tierId", QString::fromStdString(threshold.tierId.value)},
                                              {"score", threshold.score}});
            QJsonArray hashes;
            for (const auto &hash: entry.hashes)
                hashes.append(QString::fromStdString(hash));
            return QJsonObject{{"id", QString::fromStdString(entry.id.value)},
                               {"name", QString::fromStdString(entry.name)},
                               {"hashes", hashes},
                               {"thresholds", thresholds}};
        }

        QJsonArray encodeEntries(const std::vector<BenchmarkScenario> &entries) {
            QJsonArray array;
            for (const auto &entry: entries) array.append(encodeEntry(entry));
            return array;
        }

        QJsonObject encodeGroup(const BenchmarkGroup &group) {
            return QJsonObject{{"id", QString::fromStdString(group.id.value)},
                               {"name", QString::fromStdString(group.name)},
                               {"color", encodeColor(group.color)},
                               {"scenarios", encodeEntries(group.scenarios)}};
        }

        QByteArray encode(const BenchmarkDefinition &def) {
            QJsonArray tiers;
            for (const auto &tier: def.tiers)
                tiers.append(QJsonObject{{"id", QString::fromStdString(tier.id.value)},
                                         {"name", QString::fromStdString(tier.name)},
                                         {"color", encodeColor(tier.color)}});
            QJsonArray categories;
            for (const auto &category: def.categories) {
                QJsonArray subcategories;
                for (const auto &sub: category.subcategories) subcategories.append(encodeGroup(sub));
                auto object = encodeGroup(category);
                object["subcategories"] = subcategories;
                categories.append(object);
            }
            const QJsonObject root{{"schemaVersion", kSchemaVersion},
                                   {"id", QString::fromStdString(def.id.value)},
                                   {"name", QString::fromStdString(def.name)},
                                   {"tiers", tiers},
                                   {"uncategorized", encodeEntries(def.uncategorized)},
                                   {"categories", categories}};
            return QJsonDocument(root).toJson(QJsonDocument::Indented);
        }

        QString digestOf(const QByteArray &bytes) {
            return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        }

        // Current on-disk digest for a managed file, or nullopt if it does not exist / cannot be read.
        std::optional<QString> currentDigest(const QString &path) {
            QFile file(path);
            if (!file.exists()) return std::nullopt;
            if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
            return digestOf(file.readAll());
        }
    }

    BenchmarkStore::BenchmarkStore(std::string directoryPath)
        : m_directory(std::move(directoryPath)) {}

    std::string BenchmarkStore::managedDirectoryPath() const { return m_directory; }

    data::BenchmarkScanResult BenchmarkStore::scan() const {
        QDir dir(QString::fromStdString(m_directory));
        if (!dir.exists() && !QDir().mkpath(dir.absolutePath()))
            return {std::nullopt, data::BenchmarkScanFailure::DirectoryUnavailable};

        data::BenchmarkLibrarySnapshot snapshot;
        auto names = dir.entryList({"*.json"}, QDir::Files, QDir::Name);  // QDir::Name = deterministic
        for (const auto &name: names) {
            QFile file(dir.absoluteFilePath(name));
            if (!file.open(QIODevice::ReadOnly)) {
                // A file that enumerated but cannot be read is a per-file problem, not a directory failure.
                snapshot.entries.push_back({name.toStdString(), {},
                                            ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}}});
                continue;
            }
            const auto bytes = file.readAll();
            snapshot.entries.push_back({name.toStdString(), digestOf(bytes).toStdString(),
                                        classifyBytes(bytes)});
        }
        return {snapshot, std::nullopt};
    }

    data::BenchmarkWriteResult BenchmarkStore::write(const domain::BenchmarkDefinition &definition,
                                                     const std::string &filename,
                                                     const std::optional<std::string> &expectedDigest) {
        QDir dir(QString::fromStdString(m_directory));
        if (!dir.exists() && !QDir().mkpath(dir.absolutePath()))
            return {std::nullopt, data::BenchmarkWriteFailure::WriteFailed};
        const auto path = dir.absoluteFilePath(QString::fromStdString(filename));

        const auto onDisk = currentDigest(path);
        // Precondition: a replace must match the admitted digest; a create must find no existing file.
        if (expectedDigest) {
            if (!onDisk || onDisk->toStdString() != *expectedDigest)
                return {std::nullopt, data::BenchmarkWriteFailure::ExternalModificationConflict};
        } else if (onDisk) {
            return {std::nullopt, data::BenchmarkWriteFailure::ExternalModificationConflict};
        }

        const auto bytes = encode(definition);
        QSaveFile save(path);  // temp sibling + atomic commit
        if (!save.open(QIODevice::WriteOnly) || save.write(bytes) != bytes.size() || !save.commit())
            return {std::nullopt, data::BenchmarkWriteFailure::WriteFailed};
        return {digestOf(bytes).toStdString(), std::nullopt};
    }

    data::BenchmarkDeleteResult BenchmarkStore::remove(const std::string &filename,
                                                       const std::string &expectedDigest) {
        const auto path = QDir(QString::fromStdString(m_directory))
                              .absoluteFilePath(QString::fromStdString(filename));
        const auto onDisk = currentDigest(path);
        if (!onDisk || onDisk->toStdString() != expectedDigest)
            return {false, data::BenchmarkWriteFailure::ExternalModificationConflict};
        if (!QFile::remove(path)) return {false, data::BenchmarkWriteFailure::WriteFailed};
        return {true, std::nullopt};
    }
}
