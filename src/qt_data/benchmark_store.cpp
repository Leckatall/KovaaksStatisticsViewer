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

        constexpr int kSchemaVersion = 1;

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

        std::optional<ScenarioEntry> decodeEntry(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"id", "name", "hash", "thresholds"}) || !object["name"].isString() ||
                !object["thresholds"].isArray())
                return std::nullopt;
            const auto id = nonEmptyIdString(object["id"]);
            if (!id) return std::nullopt;
            std::optional<std::string> hash;
            if (!object["hash"].isNull()) {
                if (!object["hash"].isString()) return std::nullopt;
                hash = object["hash"].toString().toStdString();
            }
            std::vector<Threshold> thresholds;
            for (const auto &item: object["thresholds"].toArray()) {
                const auto threshold = decodeThreshold(item);
                if (!threshold) return std::nullopt;
                thresholds.push_back(*threshold);
            }
            return ScenarioEntry{ScenarioEntryId{*id}, object["name"].toString().toStdString(),
                                 hash, std::move(thresholds)};
        }

        std::optional<std::vector<ScenarioEntry>> decodeEntries(const QJsonValue &value) {
            if (!value.isArray()) return std::nullopt;
            std::vector<ScenarioEntry> entries;
            for (const auto &item: value.toArray()) {
                const auto entry = decodeEntry(item);
                if (!entry) return std::nullopt;
                entries.push_back(*entry);
            }
            return entries;
        }

        std::optional<Category> decodeCategory(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"id", "name", "color", "scenarios", "subcategories"}) ||
                !object["name"].isString() || !object["subcategories"].isArray())
                return std::nullopt;
            const auto id = nonEmptyIdString(object["id"]);
            const auto color = decodeColor(object["color"]);
            const auto scenarios = decodeEntries(object["scenarios"]);
            if (!id || !color || !scenarios) return std::nullopt;
            std::vector<Subcategory> subcategories;
            for (const auto &item: object["subcategories"].toArray()) {
                if (!item.isObject()) return std::nullopt;
                const auto sub = item.toObject();
                if (!exactKeys(sub, {"id", "name", "color", "scenarios"}) || !sub["name"].isString())
                    return std::nullopt;
                const auto subId = nonEmptyIdString(sub["id"]);
                const auto subColor = decodeColor(sub["color"]);
                const auto subScenarios = decodeEntries(sub["scenarios"]);
                if (!subId || !subColor || !subScenarios) return std::nullopt;
                subcategories.push_back(Subcategory{GroupId{*subId}, sub["name"].toString().toStdString(),
                                                    *subColor, std::move(*subScenarios)});
            }
            return Category{GroupId{*id}, object["name"].toString().toStdString(), *color,
                            std::move(*scenarios), std::move(subcategories)};
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
        std::optional<Benchmark> decodeStructure(const QJsonObject &root) {
            if (!exactKeys(root, {"schemaVersion", "id", "name", "tiers", "uncategorized", "categories"}) ||
                !root["name"].isString() || !root["tiers"].isArray() || !root["uncategorized"].isArray() ||
                !root["categories"].isArray())
                return std::nullopt;
            const auto id = nonEmptyIdString(root["id"]);
            if (!id) return std::nullopt;
            Benchmark def;
            def.id = BenchmarkId{*id};
            def.name = root["name"].toString().toStdString();
            for (const auto &item: root["tiers"].toArray()) {
                const auto tier = decodeTier(item);
                if (!tier) return std::nullopt;
                def.tiers.push_back(*tier);
            }
            auto uncategorized = decodeEntries(root["uncategorized"]);
            if (!uncategorized) return std::nullopt;
            def.uncategorized = std::move(*uncategorized);
            for (const auto &item: root["categories"].toArray()) {
                const auto category = decodeCategory(item);
                if (!category) return std::nullopt;
                def.categories.push_back(*category);
            }
            return def;
        }

        // Representation integrity that Invalidates a file: duplicate stable ids anywhere, and any
        // threshold referencing a tier the definition does not declare.
        bool hasStructuralIntegrity(const Benchmark &def) {
            std::unordered_set<std::string> ids;
            const auto add = [&](const std::string &value) { return ids.insert(value).second; };
            if (!add(def.id.value)) return false;
            std::unordered_set<std::string> tierIds;
            for (const auto &tier: def.tiers) {
                if (!add(tier.id.value)) return false;
                tierIds.insert(tier.id.value);
            }
            std::vector<const ScenarioEntry *> entries;
            for (const auto &e: def.uncategorized) entries.push_back(&e);
            for (const auto &category: def.categories) {
                if (!add(category.id.value)) return false;
                for (const auto &e: category.scenarios) entries.push_back(&e);
                for (const auto &sub: category.subcategories) {
                    if (!add(sub.id.value)) return false;
                    for (const auto &e: sub.scenarios) entries.push_back(&e);
                }
            }
            for (const auto *entry: entries) {
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
            if (static_cast<int>(version) != kSchemaVersion)
                return ProblemBenchmark{BenchmarkFileProblem::Invalid, displayName, embeddedId, {}};

            const auto structure = decodeStructure(root);
            if (!structure || !hasStructuralIntegrity(*structure))
                return ProblemBenchmark{BenchmarkFileProblem::Invalid, displayName, embeddedId, {}};
            return LoadedBenchmark{*structure, validateBenchmark(*structure)};
        }

        QJsonObject encodeEntry(const ScenarioEntry &entry) {
            QJsonArray thresholds;
            for (const auto &threshold: entry.thresholds)
                thresholds.append(QJsonObject{{"tierId", QString::fromStdString(threshold.tierId.value)},
                                              {"score", threshold.score}});
            return QJsonObject{{"id", QString::fromStdString(entry.id.value)},
                               {"name", QString::fromStdString(entry.name)},
                               {"hash", entry.hash ? QJsonValue(QString::fromStdString(*entry.hash)) : QJsonValue()},
                               {"thresholds", thresholds}};
        }

        QJsonArray encodeEntries(const std::vector<ScenarioEntry> &entries) {
            QJsonArray array;
            for (const auto &entry: entries) array.append(encodeEntry(entry));
            return array;
        }

        QByteArray encode(const Benchmark &def) {
            QJsonArray tiers;
            for (const auto &tier: def.tiers)
                tiers.append(QJsonObject{{"id", QString::fromStdString(tier.id.value)},
                                         {"name", QString::fromStdString(tier.name)},
                                         {"color", encodeColor(tier.color)}});
            QJsonArray categories;
            for (const auto &category: def.categories) {
                QJsonArray subcategories;
                for (const auto &sub: category.subcategories)
                    subcategories.append(QJsonObject{{"id", QString::fromStdString(sub.id.value)},
                                                     {"name", QString::fromStdString(sub.name)},
                                                     {"color", encodeColor(sub.color)},
                                                     {"scenarios", encodeEntries(sub.scenarios)}});
                categories.append(QJsonObject{{"id", QString::fromStdString(category.id.value)},
                                              {"name", QString::fromStdString(category.name)},
                                              {"color", encodeColor(category.color)},
                                              {"scenarios", encodeEntries(category.scenarios)},
                                              {"subcategories", subcategories}});
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

    data::BenchmarkWriteResult BenchmarkStore::write(const domain::Benchmark &definition,
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
