#include "series_expression_qml.h"

#include <cmath>
#include <initializer_list>
#include <type_traits>

namespace ksv::presentation {
    namespace {
        std::optional<application::PrimitiveMetric> metricFromString(const QString &value) {
            if (value == "score") return application::PrimitiveMetric::Score;
            if (value == "shots") return application::PrimitiveMetric::Shots;
            if (value == "hits") return application::PrimitiveMetric::Hits;
            if (value == "kills") return application::PrimitiveMetric::Kills;
            if (value == "dmg") return application::PrimitiveMetric::Dmg;
            return std::nullopt;
        }

        bool exactKeys(const QVariantMap &map, const std::initializer_list<const char *> expected) {
            if (map.size() != static_cast<int>(expected.size())) return false;
            for (const auto *key: expected) if (!map.contains(key)) return false;
            return true;
        }
    }

    std::optional<application::Expression> parseExpression(const QVariantMap &map) {
        const auto kind = map.value("kind").toString();
        if (kind == "primitive") {
            if (!exactKeys(map, {"kind", "primitiveMetric"})) return std::nullopt;
            const auto metric = metricFromString(map.value("primitiveMetric").toString());
            if (!metric) return std::nullopt;
            return application::primitive(*metric);
        }
        if (kind == "constant") {
            if (!exactKeys(map, {"kind", "value"})) return std::nullopt;
            bool ok = false;
            const auto value = map.value("value").toDouble(&ok);
            if (!ok || !std::isfinite(value)) return std::nullopt;
            return application::numericConstant(value);
        }
        if (kind == "runningSum" || kind == "projectedFinalValue" || kind == "projectRateToFinal" || kind == "rollingMean") {
            if (!exactKeys(map, kind == "rollingMean"
                                    ? std::initializer_list<const char *>{"kind", "input", "window"}
                                    : std::initializer_list<const char *>{"kind", "input"})) return std::nullopt;
            const auto input = parseExpression(map.value("input").toMap());
            if (!input) return std::nullopt;
            if (kind == "runningSum") return application::runningSum(*input);
            if (kind == "projectedFinalValue") return application::projectedFinalValue(*input);
            if (kind == "projectRateToFinal") return application::projectRateToFinal(*input);
            bool ok = false;
            const auto window = map.value("window").toUInt(&ok);
            if (!ok || window == 0) return std::nullopt;
            return application::rollingMean(*input, window);
        }
        if (kind == "add" || kind == "subtract" || kind == "multiply" || kind == "divide") {
            if (!exactKeys(map, {"kind", "left", "right"})) return std::nullopt;
            const auto left = parseExpression(map.value("left").toMap());
            const auto right = parseExpression(map.value("right").toMap());
            if (!left || !right) return std::nullopt;
            if (kind == "add") return application::add(*left, *right);
            if (kind == "subtract") return application::subtract(*left, *right);
            if (kind == "multiply") return application::multiply(*left, *right);
            return application::divide(*left, *right);
        }
        if (kind == "averageAcrossRuns") {
            if (!exactKeys(map, {"kind", "input", "selection"})) return std::nullopt;
            const auto input = parseExpression(map.value("input").toMap());
            const auto selection = map.value("selection").toMap();
            if (!input || selection.size() != 2 || !selection.contains("kind")) return std::nullopt;
            bool ok = false;
            if (selection.value("kind").toString() == "recentRuns" && selection.contains("count")) {
                const auto count = selection.value("count").toUInt(&ok);
                if (ok && count > 0) return application::averageAcrossRuns(*input, application::RecentRuns{count});
            } else if (selection.value("kind").toString() == "topPercentile" && selection.contains("percent")) {
                const auto percent = selection.value("percent").toDouble(&ok);
                if (ok && std::isfinite(percent)) return application::averageAcrossRuns(
                    *input, application::TopPercentile{percent});
            }
        }
        return std::nullopt;
    }

    QVariantMap expressionMap(const application::Expression &expression) {
        if (!expression) return {};
        return std::visit([](const auto &node) -> QVariantMap {
            using Node = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<Node, application::PrimitiveReference>) {
                const auto names = std::array{"score", "shots", "hits", "kills", "dmg"};
                return {{"kind", "primitive"}, {"primitiveMetric", names[static_cast<int>(node.metric)]}};
            } else if constexpr (std::is_same_v<Node, application::NumericConstant>) return {{"kind", "constant"}, {"value", node.value}};
            else if constexpr (std::is_same_v<Node, application::RunningSum>) return {{"kind", "runningSum"}, {"input", expressionMap(node.input)}};
            else if constexpr (std::is_same_v<Node, application::ProjectedFinalValue>) return {{"kind", "projectedFinalValue"}, {"input", expressionMap(node.input)}};
            else if constexpr (std::is_same_v<Node, application::ProjectRateToFinal>) return {{"kind", "projectRateToFinal"}, {"input", expressionMap(node.input)}};
            else if constexpr (std::is_same_v<Node, application::RollingMean>) return {{"kind", "rollingMean"}, {"input", expressionMap(node.input)}, {"window", node.window}};
            else if constexpr (std::is_same_v<Node, application::Add>) return {{"kind", "add"}, {"left", expressionMap(node.left)}, {"right", expressionMap(node.right)}};
            else if constexpr (std::is_same_v<Node, application::Subtract>) return {{"kind", "subtract"}, {"left", expressionMap(node.left)}, {"right", expressionMap(node.right)}};
            else if constexpr (std::is_same_v<Node, application::Multiply>) return {{"kind", "multiply"}, {"left", expressionMap(node.left)}, {"right", expressionMap(node.right)}};
            else if constexpr (std::is_same_v<Node, application::Divide>) return {{"kind", "divide"}, {"left", expressionMap(node.left)}, {"right", expressionMap(node.right)}};
            else if constexpr (std::is_same_v<Node, application::AverageAcrossRuns>) {
                QVariantMap selection;
                std::visit([&](const auto &value) {
                    using Selection = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Selection, application::RecentRuns>) selection = {{"kind", "recentRuns"}, {"count", value.count}};
                    else selection = {{"kind", "topPercentile"}, {"percent", value.percent}};
                }, node.selection);
                return QVariantMap{{"kind", "averageAcrossRuns"}, {"input", expressionMap(node.input)}, {"selection", selection}};
            } else return QVariantMap{};
        }, expression->value());
    }

    QVariantMap mutationMap(const application::MutationResult &result) {
        QVariantMap map;
        map["succeeded"] = result.succeeded();
        map["requiresReload"] = result.requiresReload;
        map["failure"] = result.failure ? QVariant(static_cast<int>(*result.failure)) : QVariant();
        map["createdId"] = result.createdId ? QVariant(QString::number(result.createdId->value)) : QVariant();
        QVariantList errors;
        for (const auto &error: result.errors)
            errors.append(QVariantMap{{"code", static_cast<int>(error.code)}, {"path", QString::fromStdString(error.path)}});
        map["validationErrors"] = errors;
        return map;
    }

    QVariantMap invalidMutationMap() {
        return {{"succeeded", false}, {"failure", "invalidRequest"}, {"requiresReload", false},
                {"createdId", QVariant()}, {"validationErrors", QVariantList{}}};
    }
}
