#include "benchmark_migration.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QString>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace ksv::qt_data::benchmark_migration {
    namespace {
        // ── JSON checks ─────────────────────────────────────────────────────────────────────────

        bool exactKeys(const QJsonObject &object, const std::initializer_list<const char *> keys) {
            if (object.size() != static_cast<qsizetype>(keys.size())) return false;
            return std::ranges::all_of(keys, [&](const char *key) { return object.contains(QLatin1String(key)); });
        }

        bool isNonEmptyString(const QJsonValue &value) { return value.isString() && !value.toString().isEmpty(); }

        // Four whole numbers from 0 to 255: red, green, blue, alpha.
        bool isColor(const QJsonValue &value) {
            if (!value.isArray() || value.toArray().size() != 4) return false;
            return std::ranges::all_of(value.toArray(), [](const QJsonValue &channel) {
                if (!channel.isDouble()) return false;
                const auto number = channel.toDouble();
                return std::floor(number) == number && number >= 0 && number <= 255;
            });
        }

        template<typename Predicate>
        bool isArrayOf(const QJsonValue &value, Predicate predicate) {
            return value.isArray() && std::ranges::all_of(value.toArray(), predicate);
        }

        // ── Schema 1 and 2 structure ────────────────────────────────────────────────────────────
        // The two schemas differ only in the scenario entry, which each version supplies.

        using EntryCheck = bool (*)(const QJsonValue &);

        bool isThreshold(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"tierId", "score"}) && isNonEmptyString(object["tierId"]) &&
                   object["score"].isDouble();
        }

        bool isTier(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"id", "name", "color"}) && isNonEmptyString(object["id"]) &&
                   object["name"].isString() && isColor(object["color"]);
        }

        bool isGroup(const QJsonObject &object, const EntryCheck isEntry) {
            return isNonEmptyString(object["id"]) && object["name"].isString() && isColor(object["color"]) &&
                   isArrayOf(object["scenarios"], isEntry);
        }

        bool isSubcategory(const QJsonValue &value, const EntryCheck isEntry) {
            return value.isObject() && exactKeys(value.toObject(), {"id", "name", "color", "scenarios"}) &&
                   isGroup(value.toObject(), isEntry);
        }

        bool isCategory(const QJsonValue &value, const EntryCheck isEntry) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"id", "name", "color", "scenarios", "subcategories"}) &&
                   isGroup(object, isEntry) && isArrayOf(object["subcategories"], [&](const QJsonValue &sub) {
                       return isSubcategory(sub, isEntry);
                   });
        }

        bool isBenchmark(const QJsonObject &root, const int version, const EntryCheck isEntry) {
            return exactKeys(root, {"schemaVersion", "id", "name", "tiers", "uncategorized", "categories"}) &&
                   root["schemaVersion"] == QJsonValue(version) && isNonEmptyString(root["id"]) &&
                   root["name"].isString() && isArrayOf(root["tiers"], isTier) &&
                   isArrayOf(root["uncategorized"], isEntry) &&
                   isArrayOf(root["categories"], [&](const QJsonValue &category) {
                       return isCategory(category, isEntry);
                   });
        }

        // ── Schema 1 entries ────────────────────────────────────────────────────────────────────

        bool isV1Entry(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"id", "name", "hash", "thresholds"}) &&
                   isNonEmptyString(object["id"]) && object["name"].isString() &&
                   (object["hash"].isNull() || object["hash"].isString()) &&
                   isArrayOf(object["thresholds"], isThreshold);
        }

        QJsonArray upgradeEntries(const QJsonValue &entries) {
            QJsonArray upgraded;
            for (const auto &value: entries.toArray()) {
                auto entry = value.toObject();
                const auto hash = entry.take("hash").toString();
                entry["hashes"] = hash.isEmpty() ? QJsonArray{} : QJsonArray{hash};
                upgraded.append(entry);
            }
            return upgraded;
        }

        QJsonObject upgradeGroup(QJsonObject group) {
            group["scenarios"] = upgradeEntries(group["scenarios"]);
            return group;
        }

        // ── Schema 2 entries ────────────────────────────────────────────────────────────────────

        bool isHashList(const QJsonValue &value) {
            QSet<QString> seen;
            return isArrayOf(value, [&](const QJsonValue &hash) {
                if (!isNonEmptyString(hash)) return false;
                const auto text = hash.toString();
                if (seen.contains(text)) return false;
                seen.insert(text);
                return true;
            });
        }

        bool isV2Entry(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"id", "name", "hashes", "thresholds"}) &&
                   isNonEmptyString(object["id"]) && object["name"].isString() && isHashList(object["hashes"]) &&
                   isArrayOf(object["thresholds"], isThreshold);
        }
    }

    // ── Steps ───────────────────────────────────────────────────────────────────────────────────

    std::optional<QJsonObject> migrateV1ToV2(const QJsonObject &v1) {
        if (!isBenchmark(v1, 1, isV1Entry)) return std::nullopt;
        auto v2 = v1;
        v2["schemaVersion"] = 2;
        v2["uncategorized"] = upgradeEntries(v1["uncategorized"]);
        QJsonArray categories;
        for (const auto &value: v1["categories"].toArray()) {
            auto category = upgradeGroup(value.toObject());
            QJsonArray subcategories;
            for (const auto &sub: category["subcategories"].toArray()) subcategories.append(upgradeGroup(sub.toObject()));
            category["subcategories"] = subcategories;
            categories.append(category);
        }
        v2["categories"] = categories;
        return v2;
    }

    std::optional<MigratedBenchmark> migrateV2ToV3(const QJsonObject &v2) {
        if (!isBenchmark(v2, 2, isV2Entry)) return std::nullopt;
        auto v3 = v2;
        v3.remove(QStringLiteral("id"));
        const auto name = v3.take(QStringLiteral("name")).toString();
        v3["schemaVersion"] = 3;
        return MigratedBenchmark{v3, name.toStdString()};
    }

    std::variant<MigratedBenchmark, BenchmarkMigrationMismatch> migrateBenchmark(const QJsonObject &stored,
                                                                                 const int version) {
        auto current = stored;
        if (version <= 1) {
            const auto v2 = migrateV1ToV2(current);
            if (!v2) return BenchmarkMigrationMismatch{1};
            current = *v2;
        }
        const auto v3 = migrateV2ToV3(current);
        if (!v3) return BenchmarkMigrationMismatch{2};
        return *v3;
    }
}
