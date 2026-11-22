#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <initializer_list>

// Format-neutral JSON checks shared by the migration steps. Which keys and types a version has
// stays in that version's step, so retiring a step touches nothing here.
namespace ksv::qt_data::benchmark_migration::json {
    inline bool exactKeys(const QJsonObject &object, const std::initializer_list<const char *> keys) {
        if (object.size() != static_cast<qsizetype>(keys.size())) return false;
        return std::ranges::all_of(keys, [&](const char *key) { return object.contains(QLatin1String(key)); });
    }

    inline bool isNonEmptyString(const QJsonValue &value) {
        return value.isString() && !value.toString().isEmpty();
    }

    // Four whole numbers from 0 to 255: red, green, blue, alpha.
    inline bool isColor(const QJsonValue &value) {
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
}
