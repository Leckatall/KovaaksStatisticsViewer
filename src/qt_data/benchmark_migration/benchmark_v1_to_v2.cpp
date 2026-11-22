#include "benchmark_v1_to_v2.h"

#include "benchmark_json_checks.h"

namespace ksv::qt_data::benchmark_migration {
    namespace {
        using namespace json;

        bool isThreshold(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"tierId", "score"}) && isNonEmptyString(object["tierId"]) &&
                   object["score"].isDouble();
        }

        bool isEntry(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"id", "name", "hash", "thresholds"}) &&
                   isNonEmptyString(object["id"]) && object["name"].isString() &&
                   (object["hash"].isNull() || object["hash"].isString()) &&
                   isArrayOf(object["thresholds"], isThreshold);
        }

        bool isGroup(const QJsonObject &object) {
            return isNonEmptyString(object["id"]) && object["name"].isString() && isColor(object["color"]) &&
                   isArrayOf(object["scenarios"], isEntry);
        }

        bool isSubcategory(const QJsonValue &value) {
            return value.isObject() && exactKeys(value.toObject(), {"id", "name", "color", "scenarios"}) &&
                   isGroup(value.toObject());
        }

        bool isCategory(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"id", "name", "color", "scenarios", "subcategories"}) &&
                   isGroup(object) && isArrayOf(object["subcategories"], isSubcategory);
        }

        bool isTier(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"id", "name", "color"}) && isNonEmptyString(object["id"]) &&
                   object["name"].isString() && isColor(object["color"]);
        }

        bool isSchemaOne(const QJsonObject &root) {
            return exactKeys(root, {"schemaVersion", "id", "name", "tiers", "uncategorized", "categories"}) &&
                   root["schemaVersion"] == QJsonValue(1) && isNonEmptyString(root["id"]) &&
                   root["name"].isString() && isArrayOf(root["tiers"], isTier) &&
                   isArrayOf(root["uncategorized"], isEntry) && isArrayOf(root["categories"], isCategory);
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
    }

    std::optional<QJsonObject> migrateV1ToV2(const QJsonObject &v1) {
        if (!isSchemaOne(v1)) return std::nullopt;
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
}
