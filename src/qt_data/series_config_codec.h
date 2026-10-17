#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "app/contracts/graph_configuration.h"

// JSON codec for the Graph Lines series section of the document stored under `graph/seriesConfigV1`.
namespace ksv::qt_data::series_codec {
    [[nodiscard]] bool exactKeys(const QJsonObject &object, std::span<const char *const> keys);

    [[nodiscard]] inline bool exactKeys(const QJsonObject &object, const std::initializer_list<const char *> keys) {
        return exactKeys(object, std::span(keys.begin(), keys.size()));
    }

    // Canonical non-zero unsigned decimal strings only: no sign, no leading zero, no JSON number.
    [[nodiscard]] std::optional<uint64_t> decimalId(const QJsonValue &value);

    // Rejects fractional, negative and out-of-range JSON numbers instead of truncating them.
    template<typename T>
    [[nodiscard]] std::optional<T> boundedInteger(const QJsonValue &value) {
        if (!value.isDouble()) return std::nullopt;
        const auto number = value.toDouble();
        if (!std::isfinite(number) || std::floor(number) != number || number < 0 ||
            number > static_cast<double>(std::numeric_limits<T>::max()))
            return std::nullopt;
        return static_cast<T>(number);
    }

    [[nodiscard]] std::optional<double> finiteNumber(const QJsonValue &value);

    [[nodiscard]] QJsonArray encodeColor(const application::RgbaColor &color);

    [[nodiscard]] std::optional<application::RgbaColor> decodeColor(const QJsonValue &value);

    // The series, axes and allocator keys of a schema-version-3 document root, without schemaVersion.
    [[nodiscard]] QJsonObject encodeRunSeries(const application::RunSeriesDocument &document);

    // `root` holds exactly the keys of the given schema version's format (1, 2 or 3); versions 1 and 2
    // migrate to the current series model.
    [[nodiscard]] std::optional<application::RunSeriesDocument> decodeRunSeries(const QJsonObject &root,
                                                                                int schemaVersion);

    // The defaults, with the series the pre-document `graph/disabledColumns` setting hid disabled.
    [[nodiscard]] application::RunSeriesDocument seededRunSeries(const std::vector<std::string> &legacyDisabledColumnKeys);
}
