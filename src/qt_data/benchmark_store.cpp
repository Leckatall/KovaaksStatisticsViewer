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
#include <set>
#include <tuple>
#include <unordered_set>

#include "benchmark_migration/benchmark_migration.h"
#include "files/file_naming.h"

namespace ksv::qt_data {
    namespace {
        using namespace data;
        using namespace ksv::domain;

        constexpr int kSchemaVersion = 3;
        constexpr std::string_view kFallbackName = "Benchmark";

        // Equal for names Windows treats as one file.
        std::string caseInsensitiveKey(const std::string_view name) {
            return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())).toCaseFolded().toStdString();
        }

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

        std::optional<BenchmarkScenario> decodeEntry(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"id", "name", "hashes", "thresholds"}) || !object["name"].isString() ||
                !object["thresholds"].isArray() || !object["hashes"].isArray())
                return std::nullopt;
            const auto id = nonEmptyIdString(object["id"]);
            if (!id) return std::nullopt;
            std::vector<std::string> hashes;
            for (const auto &item: object["hashes"].toArray()) {
                const auto hash = nonEmptyIdString(item);
                if (!hash || std::ranges::find(hashes, *hash) != hashes.end()) return std::nullopt;
                hashes.push_back(*hash);
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

        std::optional<std::vector<BenchmarkScenario>> decodeEntries(const QJsonValue &value) {
            if (!value.isArray()) return std::nullopt;
            std::vector<BenchmarkScenario> entries;
            for (const auto &item: value.toArray()) {
                const auto entry = decodeEntry(item);
                if (!entry) return std::nullopt;
                entries.push_back(*entry);
            }
            return entries;
        }

        // The caller checks the object's exact key set, which differs between categories and subcategories.
        std::optional<BenchmarkGroup> decodeGroup(const QJsonObject &object) {
            if (!object["name"].isString()) return std::nullopt;
            const auto id = nonEmptyIdString(object["id"]);
            const auto color = decodeColor(object["color"]);
            auto scenarios = decodeEntries(object["scenarios"]);
            if (!id || !color || !scenarios) return std::nullopt;
            return BenchmarkGroup{GroupId{*id}, object["name"].toString().toStdString(), *color, std::move(*scenarios)};
        }

        std::optional<Category> decodeCategory(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"id", "name", "color", "scenarios", "subcategories"}) ||
                !object["subcategories"].isArray())
                return std::nullopt;
            auto group = decodeGroup(object);
            if (!group) return std::nullopt;
            std::vector<Subcategory> subcategories;
            for (const auto &item: object["subcategories"].toArray()) {
                if (!item.isObject()) return std::nullopt;
                const auto sub = item.toObject();
                if (!exactKeys(sub, {"id", "name", "color", "scenarios"})) return std::nullopt;
                auto subGroup = decodeGroup(sub);
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
        std::optional<BenchmarkDefinition> decodeStructure(const QJsonObject &root) {
            if (!exactKeys(root, {"schemaVersion", "tiers", "uncategorized", "categories"}) ||
                !root["tiers"].isArray() || !root["uncategorized"].isArray() || !root["categories"].isArray())
                return std::nullopt;
            BenchmarkDefinition def;
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
        bool hasStructuralIntegrity(const BenchmarkDefinition &def) {
            std::unordered_set<std::string> ids;
            const auto add = [&](const std::string &value) { return ids.insert(value).second; };
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

        ProblemBenchmark invalid() { return ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}}; }

        std::variant<LoadedBenchmark, ProblemBenchmark> readCurrentFormat(const QJsonObject &root) {
            const auto structure = decodeStructure(root);
            if (!structure || !hasStructuralIntegrity(*structure)) return invalid();
            return LoadedBenchmark{*structure, validateBenchmark(*structure)};
        }

        ProblemBenchmark migrationFailed(const int fromVersion, const BenchmarkMigrationFailureReason reason) {
            return ProblemBenchmark{BenchmarkFileProblem::MigrationFailed, {}, BenchmarkMigrationFailure{fromVersion, reason}};
        }

        // A schema 1 or 2 file whose upgrade the schema 3 reader accepts, still to be renamed and written.
        struct AwaitingWrite {
            QJsonObject schema3;
            std::string storedName;
            LoadedBenchmark content;
        };

        using StoredRead = std::variant<LoadedBenchmark, ProblemBenchmark, AwaitingWrite>;

        StoredRead upgrade(const QJsonObject &root, const int version) {
            const auto migrated = benchmark_migration::migrateBenchmark(root, version);
            if (const auto *mismatch = std::get_if<benchmark_migration::BenchmarkMigrationMismatch>(&migrated))
                return migrationFailed(mismatch->fromVersion, BenchmarkMigrationFailureReason::SourceMismatch);
            const auto &upgraded = std::get<benchmark_migration::MigratedBenchmark>(migrated);
            auto read = readCurrentFormat(upgraded.schema3);
            auto *content = std::get_if<LoadedBenchmark>(&read);
            if (!content) return migrationFailed(2, BenchmarkMigrationFailureReason::InvalidResult);
            return AwaitingWrite{upgraded.schema3, upgraded.storedName, std::move(*content)};
        }

        // The only place current-format code meets older schema versions: it hands them to migration.
        StoredRead readStoredBenchmark(const QByteArray &bytes) {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) return invalid();
            const auto root = document.object();
            if (!root["schemaVersion"].isDouble()) return invalid();
            const auto version = root["schemaVersion"].toDouble();
            if (std::floor(version) != version || version < 1) return invalid();
            if (version > kSchemaVersion)
                return ProblemBenchmark{BenchmarkFileProblem::Unsupported, static_cast<int>(version), {}};
            if (version == kSchemaVersion) {
                auto current = readCurrentFormat(root);
                if (auto *loaded = std::get_if<LoadedBenchmark>(&current)) return std::move(*loaded);
                return std::get<ProblemBenchmark>(current);
            }
            return upgrade(root, static_cast<int>(version));
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

    BenchmarkStore::FileOperations BenchmarkStore::defaultFileOperations() {
        return {
            [](const QString &from, const QString &to) { return QFile::rename(from, to); },
            [](const QString &path, const QByteArray &bytes) {
                QSaveFile save(path);  // temp sibling + atomic commit
                return save.open(QIODevice::WriteOnly) && save.write(bytes) == bytes.size() && save.commit();
            },
        };
    }

    BenchmarkStore::BenchmarkStore(std::string directoryPath)
        : BenchmarkStore(std::move(directoryPath), defaultFileOperations()) {}

    BenchmarkStore::BenchmarkStore(std::string directoryPath, FileOperations operations)
        : m_directory(std::move(directoryPath)), m_operations(std::move(operations)) {}

    std::string BenchmarkStore::managedDirectoryPath() const { return m_directory; }

    data::BenchmarkScanResult BenchmarkStore::scan() {
        QDir dir(QString::fromStdString(m_directory));
        if (!dir.exists() && !QDir().mkpath(dir.absolutePath()))
            return {std::nullopt, data::BenchmarkScanFailure::DirectoryUnavailable};

        struct Pending {
            std::string filename;
            std::string digest;
            AwaitingWrite upgrade;
        };
        data::BenchmarkLibrarySnapshot snapshot;
        std::vector<Pending> pending;
        // Names of every file present when the scan starts stay taken for the whole scan, so no
        // migrating file claims a name another one is only about to leave.
        std::set<std::string> taken;
        const auto foldedName = [](const std::string &filename) {
            return caseInsensitiveKey(files::stemOf(filename, data::kBenchmarkFileExtension));
        };

        for (const auto &name: dir.entryList({"*.json"}, QDir::Files)) {
            const auto filename = name.toStdString();
            taken.insert(foldedName(filename));
            QFile file(dir.absoluteFilePath(name));
            if (!file.open(QIODevice::ReadOnly)) {
                // A file that enumerated but cannot be read is a per-file problem, not a directory failure.
                snapshot.entries.push_back({filename, {}, invalid()});
                continue;
            }
            const auto bytes = file.readAll();
            auto digest = digestOf(bytes).toStdString();
            auto read = readStoredBenchmark(bytes);
            if (auto *upgrade = std::get_if<AwaitingWrite>(&read)) {
                pending.push_back({filename, std::move(digest), std::move(*upgrade)});
                continue;
            }
            if (auto *loaded = std::get_if<LoadedBenchmark>(&read))
                snapshot.entries.push_back({filename, std::move(digest), std::move(*loaded)});
            else
                snapshot.entries.push_back({filename, std::move(digest), std::get<ProblemBenchmark>(read)});
        }

        std::ranges::sort(pending, {}, [&](const Pending &file) {
            return std::tuple{foldedName(file.filename), file.filename};
        });
        for (auto &file: pending) {
            const auto ownName = foldedName(file.filename);
            const auto claimed = files::uniqueName(
                files::cleanName(file.upgrade.storedName, data::kBenchmarkFileExtension, kFallbackName),
                data::kBenchmarkFileExtension, kFallbackName, [&](const std::string_view candidate) {
                    const auto folded = caseInsensitiveKey(candidate);
                    return folded != ownName && taken.contains(folded);
                });
            taken.insert(caseInsensitiveKey(claimed));

            // Renamed before it is rewritten, so a failure never leaves the new format under the old
            // name, where no later scan would migrate it and its stored name would be lost.
            auto target = file.filename;
            if (claimed != files::stemOf(file.filename, data::kBenchmarkFileExtension)) {
                target = claimed + std::string{data::kBenchmarkFileExtension};
                if (!m_operations.rename(dir.absoluteFilePath(QString::fromStdString(file.filename)),
                                         dir.absoluteFilePath(QString::fromStdString(target)))) {
                    snapshot.entries.push_back({file.filename, file.digest,
                                                migrationFailed(2, BenchmarkMigrationFailureReason::RenameFailed)});
                    continue;
                }
            }
            const auto bytes = QJsonDocument(file.upgrade.schema3).toJson(QJsonDocument::Indented);
            if (!m_operations.writeAtomically(dir.absoluteFilePath(QString::fromStdString(target)), bytes)) {
                snapshot.entries.push_back({target, file.digest,
                                            migrationFailed(2, BenchmarkMigrationFailureReason::WriteFailed)});
                continue;
            }
            snapshot.entries.push_back({target, digestOf(bytes).toStdString(), std::move(file.upgrade.content)});
        }

        std::ranges::sort(snapshot.entries, {}, &data::BenchmarkFileEntry::filename);
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
        if (!m_operations.writeAtomically(path, bytes)) return {std::nullopt, data::BenchmarkWriteFailure::WriteFailed};
        return {digestOf(bytes).toStdString(), std::nullopt};
    }

    data::BenchmarkRenameResult BenchmarkStore::rename(const std::string &from, const std::string &to,
                                                       const std::string &expectedDigest) {
        const QDir dir(QString::fromStdString(m_directory));
        const auto fromPath = dir.absoluteFilePath(QString::fromStdString(from));
        const auto onDisk = currentDigest(fromPath);
        if (!onDisk || onDisk->toStdString() != expectedDigest)
            return {false, data::BenchmarkWriteFailure::ExternalModificationConflict};
        // QFile::rename refuses an existing target unless it is this same file respelt.
        if (!m_operations.rename(fromPath, dir.absoluteFilePath(QString::fromStdString(to))))
            return {false, data::BenchmarkWriteFailure::WriteFailed};
        return {true, std::nullopt};
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
