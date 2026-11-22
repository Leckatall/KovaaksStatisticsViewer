#include "benchmark_v2_to_v3.h"

#include <QSet>
#include <QString>

#include "benchmark_json_checks.h"

namespace ksv::qt_data::benchmark_migration {
    namespace {
        using namespace json;

        bool isThreshold(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"tierId", "score"}) && isNonEmptyString(object["tierId"]) &&
                   object["score"].isDouble();
        }

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

        bool isEntry(const QJsonValue &value) {
            const auto object = value.toObject();
            return value.isObject() && exactKeys(object, {"id", "name", "hashes", "thresholds"}) &&
                   isNonEmptyString(object["id"]) && object["name"].isString() && isHashList(object["hashes"]) &&
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

        bool isSchemaTwo(const QJsonObject &root) {
            return exactKeys(root, {"schemaVersion", "id", "name", "tiers", "uncategorized", "categories"}) &&
                   root["schemaVersion"] == QJsonValue(2) && isNonEmptyString(root["id"]) &&
                   root["name"].isString() && isArrayOf(root["tiers"], isTier) &&
                   isArrayOf(root["uncategorized"], isEntry) && isArrayOf(root["categories"], isCategory);
        }
    }

    std::optional<BenchmarkV2ToV3Result> migrateV2ToV3(const QJsonObject &v2) {
        if (!isSchemaTwo(v2)) return std::nullopt;
        auto v3 = v2;
        v3.remove(QStringLiteral("id"));
        const auto name = v3.take(QStringLiteral("name")).toString();
        v3["schemaVersion"] = 3;
        return BenchmarkV2ToV3Result{v3, name.toStdString()};
    }
}
