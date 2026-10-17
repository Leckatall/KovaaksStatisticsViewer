#include "series_config_codec.h"

#include "app/contracts/expression_dsl.h"

#include <QString>

#include <algorithm>
#include <array>
#include <utility>

namespace ksv::qt_data::series_codec {
    namespace {
        using namespace application;

        std::optional<PrimitiveMetric> metricFromTag(const QString &tag) {
            if (tag == "score") return PrimitiveMetric::Score;
            if (tag == "shots") return PrimitiveMetric::Shots;
            if (tag == "hits") return PrimitiveMetric::Hits;
            if (tag == "kills") return PrimitiveMetric::Kills;
            if (tag == "dmg") return PrimitiveMetric::Dmg;
            return std::nullopt;
        }

        std::optional<Expression> decodeExpression(const QJsonValue &value);

        QJsonObject encodeStyle(const LineStyle &style) {
            return {{"color", encodeColor(style.color)}, {"width", style.width}};
        }

        std::optional<LineStyle> decodeStyle(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"color", "width"})) return std::nullopt;
            const auto color = decodeColor(object["color"]);
            const auto width = finiteNumber(object["width"]);
            if (!color || !width) return std::nullopt;
            return LineStyle{*color, *width};
        }

        QJsonObject encodePresentation(const SeriesPresentation &presentation) {
            return {
                {"name", QString::fromStdString(presentation.name)}, {"enabled", presentation.enabled},
                {"displayPosition", static_cast<double>(presentation.displayPosition)},
                {"lineStyle", encodeStyle(presentation.lineStyle)}
            };
        }

        std::optional<SeriesPresentation> decodePresentation(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"name", "enabled", "displayPosition", "lineStyle"}) || !object["name"].isString() ||
                !object["enabled"].isBool())
                return std::nullopt;
            const auto position = boundedInteger<uint32_t>(object["displayPosition"]);
            const auto style = decodeStyle(object["lineStyle"]);
            if (!position || !style) return std::nullopt;
            return SeriesPresentation{
                object["name"].toString().toStdString(), *style, object["enabled"].toBool(), *position
            };
        }

        using BinaryExpressionFactory = Expression (*)(Expression, Expression);
        using UnaryExpressionFactory = Expression (*)(Expression);

        std::optional<Expression> decodeBinaryExpression(const QJsonObject &object,
                                                         const BinaryExpressionFactory factory) {
            if (!exactKeys(object, {"kind", "left", "right"})) return {};
            const auto left = decodeExpression(object["left"]);
            const auto right = decodeExpression(object["right"]);
            if (!left || !right) return {};
            return factory(*left, *right);
        }

        std::optional<Expression>
        decodeUnaryExpression(const QJsonObject &object, const UnaryExpressionFactory factory) {
            if (!exactKeys(object, {"kind", "input"})) return {};
            const auto input = decodeExpression(object["input"]);
            return input ? std::optional{factory(*input)} : std::nullopt;
        }

        std::optional<RunSelection> decodeSelection(const QJsonValue &value) {
            if (!value.isObject()) return {};
            const auto selection = value.toObject();
            if (!selection["kind"].isString()) return {};
            if (selection["kind"] == "recentRuns") {
                const auto count = boundedInteger<uint32_t>(selection["count"]);
                if (!exactKeys(selection, {"kind", "count"}) || !count) return {};
                return RecentRuns{*count};
            }
            const auto percent = finiteNumber(selection["percent"]);
            if (!exactKeys(selection, {"kind", "percent"}) || selection["kind"] != "topPercentile" || !percent)
                return {};
            return TopPercentile{*percent};
        }

        // Version 2's nested-object expressions; version 3 stores the DSL text instead.
        std::optional<Expression> decodeExpression(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!object["kind"].isString()) return std::nullopt;
            const auto kind = object["kind"].toString();
            if (kind == "primitive") {
                const auto metric = metricFromTag(object["primitiveMetric"].toString());
                if (!exactKeys(object, {"kind", "primitiveMetric"}) || !metric) return {};
                return primitive(*metric);
            }
            if (kind == "constant") {
                const auto number = finiteNumber(object["value"]);
                if (!exactKeys(object, {"kind", "value"}) || !number) return {};
                return numericConstant(*number);
            }
            if (kind == "add") return decodeBinaryExpression(object, add);
            if (kind == "subtract") return decodeBinaryExpression(object, subtract);
            if (kind == "multiply") return decodeBinaryExpression(object, multiply);
            if (kind == "divide") return decodeBinaryExpression(object, divide);
            if (kind == "runningSum") return decodeUnaryExpression(object, runningSum);
            if (kind == "projectedFinalValue") return decodeUnaryExpression(object, projectedFinalValue);
            if (kind == "projectRateToFinal") return decodeUnaryExpression(object, projectRateToFinal);
            if (kind == "rollingMean") {
                if (!exactKeys(object, {"kind", "input", "window"})) return {};
                const auto decoded = decodeExpression(object["input"]);
                const auto window = boundedInteger<uint32_t>(object["window"]);
                if (!decoded || !window) return {};
                return rollingMean(*decoded, *window);
            }
            if (kind != "averageAcrossRuns" || !exactKeys(object, {"kind", "input", "selection"})) return {};
            const auto input = decodeExpression(object["input"]);
            const auto selection = decodeSelection(object["selection"]);
            if (!input || !selection) return {};
            return averageAcrossRuns(*input, *selection);
        }

        QString transformKindTag(const AxisTransformKind kind) {
            switch (kind) {
                case AxisTransformKind::Identity: return "identity";
                case AxisTransformKind::Percentage: return "percentage";
            }
            return {};
        }

        std::optional<AxisTransformKind> transformKindFromTag(const QString &tag) {
            if (tag == "identity") return AxisTransformKind::Identity;
            if (tag == "percentage") return AxisTransformKind::Percentage;
            return std::nullopt;
        }

        QString baselineTag(const ValueAxisOptions::Baseline baseline) {
            switch (baseline) {
                case ValueAxisOptions::Baseline::Zero: return "zero";
                case ValueAxisOptions::Baseline::HugData: return "hugData";
            }
            return {};
        }

        std::optional<ValueAxisOptions::Baseline> baselineFromTag(const QString &tag) {
            if (tag == "zero") return ValueAxisOptions::Baseline::Zero;
            if (tag == "hugData") return ValueAxisOptions::Baseline::HugData;
            return std::nullopt;
        }

        QJsonObject encodeAxisOptions(const ValueAxisOptions &options) {
            return {{"baseline", baselineTag(options.baseline)}, {"integral", options.integral},
                    {"targetTicks", options.targetTicks}, {"fallbackSpan", options.fallbackSpan}};
        }

        std::optional<ValueAxisOptions> decodeAxisOptions(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"baseline", "integral", "targetTicks", "fallbackSpan"}) ||
                !object["baseline"].isString() || !object["integral"].isBool())
                return std::nullopt;
            const auto baseline = baselineFromTag(object["baseline"].toString());
            const auto targetTicks = boundedInteger<int>(object["targetTicks"]);
            const auto fallbackSpan = finiteNumber(object["fallbackSpan"]);
            if (!baseline || !targetTicks || !fallbackSpan || *fallbackSpan <= 0.0) return std::nullopt;
            return ValueAxisOptions{*baseline, object["integral"].toBool(), *targetTicks, *fallbackSpan};
        }

        QJsonObject encodeAxis(const AxisConfig &axis) {
            return {{"id", QString::number(axis.id.value)}, {"name", QString::fromStdString(axis.name)},
                    {"options", encodeAxisOptions(axis.options)}, {"transformKind", transformKindTag(axis.transformKind)}};
        }

        std::optional<AxisConfig> decodeAxis(const QJsonValue &value) {
            if (!value.isObject()) return std::nullopt;
            const auto object = value.toObject();
            if (!exactKeys(object, {"id", "name", "options", "transformKind"}) || !object["name"].isString() ||
                !object["transformKind"].isString()) return std::nullopt;
            const auto id = decimalId(object["id"]);
            const auto options = decodeAxisOptions(object["options"]);
            const auto transformKind = transformKindFromTag(object["transformKind"].toString());
            if (!id || !options || !transformKind) return std::nullopt;
            return AxisConfig{{*id}, object["name"].toString().toStdString(), *options, *transformKind};
        }

        std::optional<std::vector<SeriesConfig>> decodeSeriesArray(const QJsonValue &value, const bool expectAxisFields,
                                                                    const bool expressionIsDsl = false) {
            if (!value.isArray()) return std::nullopt;
            std::vector<SeriesConfig> configs;
            for (const auto &item: value.toArray()) {
                if (!item.isObject()) return std::nullopt;
                const auto object = item.toObject();
                const auto presentation = decodePresentation(object["presentation"]);
                const auto id = decimalId(object["id"]);
                if (!presentation || !id) return std::nullopt;
                Expression expression;
                if (expressionIsDsl) {
                    if (!object["expression"].isString()) return std::nullopt;
                    const auto decoded = decodeExpressionDsl(object["expression"].toString().toStdString());
                    if (!decoded) return std::nullopt;
                    expression = *decoded;
                } else if (!object["expression"].isNull()) {
                    const auto decoded = decodeExpression(object["expression"]);
                    if (!decoded) return std::nullopt;
                    expression = *decoded;
                }
                std::optional<AxisId> yAxisId;
                auto transformKind = AxisTransformKind::Identity;
                if (expectAxisFields) {
                    if (!exactKeys(object, {"id", "presentation", "expression", "yAxisId", "transformKind"}) ||
                        !object["transformKind"].isString()) return std::nullopt;
                    if (!object["yAxisId"].isNull()) {
                        const auto axisId = decimalId(object["yAxisId"]);
                        if (!axisId) return std::nullopt;
                        yAxisId = AxisId{*axisId};
                    }
                    const auto kind = transformKindFromTag(object["transformKind"].toString());
                    if (!kind) return std::nullopt;
                    transformKind = *kind;
                } else if (!exactKeys(object, {"id", "presentation", "expression"})) return std::nullopt;
                configs.emplace_back(SeriesConfig{{*id}, *presentation, expression, yAxisId, transformKind});
            }
            return configs;
        }

        std::optional<std::optional<uint64_t>> optionalDecimalId(const QJsonValue &value) {
            if (value.isNull()) return std::optional<uint64_t>{};
            const auto id = decimalId(value);
            return id ? std::optional{*id} : std::nullopt;
        }

        RunSeriesDocument migrateV1ToV2(std::vector<SeriesConfig> configs, const SeriesId nextSeriesId) {
            auto axes = defaultAxisConfigs();
            for (auto &config: configs) {
                if (config.id.value == 7 || config.id.value == 8 || config.id.value == 9) config.yAxisId = axes[1].id;
                config.transformKind = config.id.value == 2 ? AxisTransformKind::Percentage : AxisTransformKind::Identity;
            }
            return {std::move(configs), nextSeriesId, std::move(axes), kFirstUserAxisId};
        }

        std::optional<RunSeriesDocument> migrateV1ToV3(const QJsonObject &root) {
            if (!exactKeys(root, {"schemaVersion", "nextComputedSeriesId", "series"})) return {};
            const auto nextId = decimalId(root["nextComputedSeriesId"]);
            auto configs = decodeSeriesArray(root["series"], false);
            if (!nextId || !configs) return {};
            return migrateV1ToV2(std::move(*configs), SeriesId{*nextId});
        }

        std::optional<RunSeriesDocument> decodeVersionedDocument(const QJsonObject &root, const bool expressionIsDsl) {
            if (!exactKeys(root, {"schemaVersion", "nextComputedSeriesId", "series", "axes", "nextAxisId"})) return {};
            const auto nextSeriesId = optionalDecimalId(root["nextComputedSeriesId"]);
            const auto nextAxisId = optionalDecimalId(root["nextAxisId"]);
            if (!nextSeriesId || !nextAxisId || !root["axes"].isArray()) return {};
            std::vector<AxisConfig> axes;
            for (const auto &item: root["axes"].toArray()) {
                const auto axis = decodeAxis(item);
                if (!axis) return {};
                axes.push_back(*axis);
            }
            auto configs = decodeSeriesArray(root["series"], true, expressionIsDsl);
            if (!configs) return {};
            const auto nextSeries = nextSeriesId->has_value() ? std::optional{SeriesId{**nextSeriesId}} : std::nullopt;
            const auto nextAxis = nextAxisId->has_value() ? std::optional{AxisId{**nextAxisId}} : std::nullopt;
            return RunSeriesDocument{std::move(*configs), nextSeries, std::move(axes), nextAxis};
        }
    }

    bool exactKeys(const QJsonObject &object, const std::span<const char *const> keys) {
        if (object.size() != static_cast<qsizetype>(keys.size())) return false; // TODO: Necessary?
        return std::ranges::all_of(keys, [&](const auto &key) { return object.contains(key); });
    }

    std::optional<uint64_t> decimalId(const QJsonValue &value) {
        if (!value.isString()) return std::nullopt;
        const auto text = value.toString();
        if (text.isEmpty() || (text.size() > 1 && text.front() == '0')) return std::nullopt;
        for (const auto c: text) if (c < '0' || c > '9') return std::nullopt;
        bool ok = false;
        const auto parsed = text.toULongLong(&ok);
        if (!ok || parsed == 0) return std::nullopt;
        return parsed;
    }

    std::optional<double> finiteNumber(const QJsonValue &value) {
        if (!value.isDouble() || !std::isfinite(value.toDouble())) return std::nullopt;
        return value.toDouble();
    }

    QJsonArray encodeColor(const RgbaColor &color) {
        return {color.red, color.green, color.blue, color.alpha};
    }

    std::optional<RgbaColor> decodeColor(const QJsonValue &value) {
        if (!value.isArray()) return std::nullopt;
        const auto color = value.toArray();
        if (color.size() != 4) return std::nullopt;
        const auto red = boundedInteger<uint8_t>(color[0]);
        const auto green = boundedInteger<uint8_t>(color[1]);
        const auto blue = boundedInteger<uint8_t>(color[2]);
        const auto alpha = boundedInteger<uint8_t>(color[3]);
        if (!red || !green || !blue || !alpha) return std::nullopt;
        return RgbaColor{*red, *green, *blue, *alpha};
    }

    QJsonObject encodeRunSeries(const RunSeriesDocument &document) {
        QJsonArray series;
        for (const auto &config: document.series)
            series.append(QJsonObject{{"id", QString::number(config.id.value)},
                {"presentation", encodePresentation(config.presentation)},
                {"expression", QString::fromStdString(encodeExpressionDsl(config.expression))},
                {"yAxisId", config.yAxisId ? QJsonValue(QString::number(config.yAxisId->value)) : QJsonValue()},
                {"transformKind", transformKindTag(config.transformKind)}});
        QJsonArray axes;
        for (const auto &axis: document.axes) axes.append(encodeAxis(axis));
        return {
            {"nextComputedSeriesId", document.nextSeriesId
                                         ? QJsonValue(QString::number(document.nextSeriesId->value))
                                         : QJsonValue()},
            {"series", series}, {"axes", axes},
            {"nextAxisId", document.nextAxisId ? QJsonValue(QString::number(document.nextAxisId->value)) : QJsonValue()}
        };
    }

    std::optional<RunSeriesDocument> decodeRunSeries(const QJsonObject &root, const int schemaVersion) {
        std::optional<RunSeriesDocument> document;
        if (schemaVersion == 1) document = migrateV1ToV3(root);
        else if (schemaVersion == 2) document = decodeVersionedDocument(root, false);
        else if (schemaVersion == 3) document = decodeVersionedDocument(root, true);
        if (!document || !validateRunSeriesDocument(*document).empty()) return {};
        return document;
    }

    RunSeriesDocument seededRunSeries(const std::vector<std::string> &legacyDisabledColumnKeys) {
        auto document = defaultRunSeriesDocument();
        constexpr std::array<std::pair<const char *, unsigned>, 8> legacy{
            {
                {"score", 0}, {"accuracy", 1}, {"shots", 2}, {"kills", 4},
                {"dmg", 5}, {"scoreTotal", 6}, {"expectedFinalScore", 7}, {"expectedFinalScoreRecent", 8}
            }
        };
        for (const auto &[legacyKey, recordIndex]: legacy) {
            if (std::ranges::find(legacyDisabledColumnKeys, std::string(legacyKey)) != legacyDisabledColumnKeys.end())
                document.series[recordIndex].presentation.enabled = false;
        }
        return document;
    }
}
