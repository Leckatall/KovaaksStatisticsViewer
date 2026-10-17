#include "graph_configuration_codec.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "series_config_codec.h"

namespace ksv::qt_data {
    namespace {
        using namespace application;
        using namespace series_codec;
        using namespace std::chrono;

        // ISO 8601 calendar dates: the stored value never depends on a timezone or locale.
        QString encodeDate(const year_month_day date) {
            char text[16];
            std::snprintf(text, sizeof text, "%04d-%02u-%02u", static_cast<int>(date.year()),
                          static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()));
            return QString::fromLatin1(text);
        }

        std::optional<year_month_day> decodeDate(const QJsonValue &value) {
            static const QRegularExpression pattern(QStringLiteral("^(\\d{4})-(\\d{2})-(\\d{2})$"));
            if (!value.isString()) return std::nullopt;
            const auto match = pattern.match(value.toString());
            if (!match.hasMatch()) return std::nullopt;
            const year_month_day date{
                year{match.captured(1).toInt()}, month{match.captured(2).toUInt()}, day{match.captured(3).toUInt()}
            };
            return date.ok() ? std::optional{date} : std::nullopt;
        }

        QJsonObject encodeRange(const GraphRange &range) {
            return std::visit([](const auto &value) -> QJsonObject {
                using Range = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<Range, AllHistory>) return {{"kind", "all"}};
                else if constexpr (std::same_as<Range, RecentDays>) return {{"kind", "recentDays"}, {"days", value.days}};
                else if constexpr (std::same_as<Range, CustomDates>)
                    return {{"kind", "customDates"}, {"start", encodeDate(value.start)}, {"end", encodeDate(value.end)}};
                else return {{"kind", "latestRuns"}, {"count", value.count}};
            }, range);
        }

        std::optional<GraphRange> decodeRange(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            const auto kind = object["kind"].toString();
            if (kind == "all" && exactKeys(object, {"kind"})) return AllHistory{};
            if (kind == "recentDays" && exactKeys(object, {"kind", "days"})) {
                const auto days = boundedInteger<int>(object["days"]);
                return days ? std::optional<GraphRange>{RecentDays{*days}} : std::nullopt;
            }
            if (kind == "latestRuns" && exactKeys(object, {"kind", "count"})) {
                const auto count = boundedInteger<int>(object["count"]);
                return count ? std::optional<GraphRange>{LatestRuns{*count}} : std::nullopt;
            }
            if (kind == "customDates" && exactKeys(object, {"kind", "start", "end"})) {
                const auto start = decodeDate(object["start"]);
                const auto end = decodeDate(object["end"]);
                return start && end ? std::optional<GraphRange>{CustomDates{*start, *end}} : std::nullopt;
            }
            return std::nullopt;
        }

        QString latin1(const std::string_view text) {
            return QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size()));
        }

        QJsonObject encodeSeriesStyle(const SeriesStyleOverride &style) {
            QJsonObject object;
            if (style.color) object["color"] = encodeColor(*style.color);
            if (style.width) object["width"] = *style.width;
            return object;
        }

        std::optional<SeriesStyleOverride> decodeSeriesStyle(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            SeriesStyleOverride style;
            for (auto it = object.begin(); it != object.end(); ++it) {
                if (it.key() == "color") {
                    style.color = decodeColor(it.value());
                    if (!style.color) return std::nullopt;
                } else if (it.key() == "width") {
                    style.width = finiteNumber(it.value());
                    if (!style.width) return std::nullopt;
                } else {
                    return std::nullopt;
                }
            }
            return style;
        }

        QJsonObject encodeAxisPolicy(const AxisPolicy &policy) {
            return std::visit([](const auto &value) -> QJsonObject {
                using Policy = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<Policy, FitVisibleData>) return {{"mode", "fitVisible"}};
                else if constexpr (std::same_as<Policy, StartAtZero>) return {{"mode", "startAtZero"}};
                else return {{"mode", "fixed"}, {"min", value.minimum}, {"max", value.maximum}};
            }, policy);
        }

        std::optional<AxisPolicy> decodeAxisPolicy(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            const auto mode = object["mode"].toString();
            if (mode == "fitVisible" && exactKeys(object, {"mode"})) return FitVisibleData{};
            if (mode == "startAtZero" && exactKeys(object, {"mode"})) return StartAtZero{};
            if (mode != "fixed" || !exactKeys(object, {"mode", "min", "max"})) return std::nullopt;
            const auto minimum = finiteNumber(object["min"]);
            const auto maximum = finiteNumber(object["max"]);
            if (!minimum || !maximum) return std::nullopt;
            return FixedBounds{*minimum, *maximum};
        }

        std::vector<const char *> graphSectionKeys(const GraphDescriptor &descriptor) {
            std::vector<const char *> keys{"connection", "lineModeMarkers", "seriesStyles", "axisPolicies"};
            if (descriptor.dateRange) keys.insert(keys.end(), {"range", "anchorToLatestRun"});
            if (descriptor.averagingWindow) keys.push_back("averagingWindowDays");
            return keys;
        }

        QJsonObject encodeGraph(const GraphPreferences &preferences) {
            QJsonObject seriesStyles;
            for (const auto &[key, style]: preferences.seriesStyles)
                seriesStyles[QString::fromStdString(key)] = encodeSeriesStyle(style);
            QJsonObject axisPolicies;
            for (const auto &[key, policy]: preferences.axisPolicies)
                axisPolicies[QString::fromStdString(key)] = encodeAxisPolicy(policy);
            QJsonObject object{
                {"connection", latin1(connectionModeName(preferences.appearance.connection))},
                {"lineModeMarkers", preferences.appearance.lineModeMarkers},
                {"seriesStyles", seriesStyles}, {"axisPolicies", axisPolicies}
            };
            if (preferences.range) {
                object["range"] = encodeRange(preferences.range->range);
                object["anchorToLatestRun"] = preferences.range->anchorToLatestRun;
            }
            if (preferences.averagingWindowDays) object["averagingWindowDays"] = *preferences.averagingWindowDays;
            return object;
        }

        std::optional<GraphPreferences> decodeGraph(const GraphKey key, const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            const auto &descriptor = graphDescriptor(key);
            if (!exactKeys(object, graphSectionKeys(descriptor)) || !object["lineModeMarkers"].isBool() ||
                !object["seriesStyles"].isObject() || !object["axisPolicies"].isObject())
                return std::nullopt;

            GraphPreferences preferences;
            const auto connection = connectionModeFromName(object["connection"].toString().toStdString());
            if (!connection) return std::nullopt;
            preferences.appearance = {*connection, object["lineModeMarkers"].toBool()};

            if (descriptor.dateRange) {
                const auto range = decodeRange(object["range"]);
                if (!range || !object["anchorToLatestRun"].isBool()) return std::nullopt;
                preferences.range = RangePreference{*range, object["anchorToLatestRun"].toBool()};
            }
            if (descriptor.averagingWindow) {
                preferences.averagingWindowDays = boundedInteger<int>(object["averagingWindowDays"]);
                if (!preferences.averagingWindowDays) return std::nullopt;
            }

            const auto seriesStyles = object["seriesStyles"].toObject();
            for (auto it = seriesStyles.begin(); it != seriesStyles.end(); ++it) {
                const auto style = decodeSeriesStyle(it.value());
                if (!style) return std::nullopt;
                preferences.seriesStyles.emplace(it.key().toStdString(), *style);
            }
            const auto axisPolicies = object["axisPolicies"].toObject();
            for (auto it = axisPolicies.begin(); it != axisPolicies.end(); ++it) {
                const auto policy = decodeAxisPolicy(it.value());
                if (!policy) return std::nullopt;
                preferences.axisPolicies.emplace(it.key().toStdString(), *policy);
            }

            if (!validateGraphPreferences(key, preferences).empty()) return std::nullopt;
            return preferences;
        }

        void decodeGraphs(const QJsonValue &value, DecodedGraphConfiguration &decoded) {
            if (!value.isObject()) {
                decoded.hasRejectedSections = true;
                return;
            }
            const auto graphs = value.toObject();
            for (auto it = graphs.begin(); it != graphs.end(); ++it) {
                const auto key = graphKeyFromName(it.key().toStdString());
                if (!key) {
                    decoded.hasRejectedSections = true;
                    continue;
                }
                if (const auto preferences = decodeGraph(*key, it.value()))
                    decoded.configuration.preferences(*key) = *preferences;
                else
                    decoded.hasRejectedSections = true;
            }
        }
    }

    std::optional<DecodedGraphConfiguration> decodeGraphConfigurationDocument(const std::string &raw) {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(raw), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) return std::nullopt;
        auto root = document.object();
        if (!root["schemaVersion"].isDouble()) return std::nullopt;
        const auto version = root["schemaVersion"].toDouble();
        if (version != 1 && version != 2 && version != 3 && version != kGraphConfigurationSchemaVersion)
            return std::nullopt;

        DecodedGraphConfiguration decoded{defaultGraphConfiguration()};
        int seriesVersion = static_cast<int>(version);
        if (seriesVersion == kGraphConfigurationSchemaVersion) {
            // Version 4 is version 3's series section plus these two keys.
            seriesVersion = 3;
            root["schemaVersion"] = seriesVersion;
            if (root.contains("runAxisLabelSeriesId")) {
                const auto label = root.take("runAxisLabelSeriesId");
                if (label.isNull()) {
                    decoded.runAxisLabelStored = true;
                } else if (const auto id = decimalId(label)) {
                    decoded.runAxisLabelStored = true;
                    decoded.configuration.runAxisLabelSeriesId = SeriesId{*id};
                } else {
                    decoded.hasRejectedSections = true;
                }
            }
            if (root.contains("graphs")) decodeGraphs(root.take("graphs"), decoded);
        }

        if (auto runSeries = decodeRunSeries(root, seriesVersion)) {
            decoded.configuration.runSeries = std::move(*runSeries);
        } else {
            decoded.hasRejectedSections = true;
            decoded.runSeriesRejected = true;
        }
        return decoded;
    }

    std::string encodeGraphConfigurationDocument(const GraphConfiguration &configuration) {
        auto root = encodeRunSeries(configuration.runSeries);
        root["schemaVersion"] = kGraphConfigurationSchemaVersion;
        root["runAxisLabelSeriesId"] = configuration.runAxisLabelSeriesId
                                           ? QJsonValue(QString::number(configuration.runAxisLabelSeriesId->value))
                                           : QJsonValue();
        QJsonObject graphs;
        for (const auto key: kGraphKeys) {
            graphs[latin1(graphKeyName(key))] = encodeGraph(configuration.preferences(key));
        }
        root["graphs"] = graphs;
        return QJsonDocument(root).toJson(QJsonDocument::Indented).toStdString();
    }
}
