#include "series_config.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <ranges>
#include <string_view>
#include <utility>

namespace ksv::application {
    namespace {
        constexpr double kMinimumLineWidth = 0.5;
        constexpr double kMaximumLineWidth = 12.0;
        constexpr size_t kMaximumComputedNameBytes = 120;
        constexpr size_t kMaximumExpressionDepth = 16;
        constexpr size_t kMaximumExpressionNodes = 256;

        [[nodiscard]] bool isKnownPrimitiveMetric(const PrimitiveMetric metric) {
            return std::ranges::find(kPrimitiveMetrics, metric) != kPrimitiveMetrics.end();
        }

        [[nodiscard]] std::string_view canonicalName(const PrimitiveMetric metric) {
            switch (metric) {
                case PrimitiveMetric::Score: return "Score";
                case PrimitiveMetric::Shots: return "Shots";
                case PrimitiveMetric::Hits: return "Hits";
                case PrimitiveMetric::Kills: return "Kills";
                case PrimitiveMetric::Dmg: return "Dmg";
            }
            return {};
        }

        [[nodiscard]] std::string_view trim(const std::string_view value) {
            constexpr auto isSpace = [](const unsigned char character) { return std::isspace(character) != 0; };
            size_t first = 0;
            while (first < value.size() && isSpace(static_cast<unsigned char>(value[first]))) ++first;
            size_t last = value.size();
            while (last > first && isSpace(static_cast<unsigned char>(value[last - 1]))) --last;
            return value.substr(first, last - first);
        }

        void validatePresentation(const SeriesPresentation &presentation, const bool computed,
                                  const std::string_view path, std::vector<ValidationError> &errors) {
            if (!std::isfinite(presentation.lineStyle.width)) {
                errors.push_back({
                    SeriesConfigValidationCode::NonFiniteLineWidth,
                    std::string(path) + ".lineStyle.width"
                });
            } else if (presentation.lineStyle.width < kMinimumLineWidth || presentation.lineStyle.width >
                       kMaximumLineWidth) {
                errors.push_back({
                    SeriesConfigValidationCode::LineWidthOutOfRange,
                    std::string(path) + ".lineStyle.width"
                });
            }
            if (!computed) return;

            const auto name = std::string_view{presentation.name};
            if (trim(name).empty()) {
                errors.push_back({SeriesConfigValidationCode::EmptyComputedName, std::string(path) + ".name"});
            } else if (trim(name) != name) {
                errors.push_back({SeriesConfigValidationCode::ComputedNameNotTrimmed, std::string(path) + ".name"});
            }
            if (name.size() > kMaximumComputedNameBytes) {
                errors.push_back({SeriesConfigValidationCode::ComputedNameTooLong, std::string(path) + ".name"});
            }
        }

        void validateExpression(const Expression &expression, const std::string &path, const size_t depth,
                                size_t &nodeCount, std::vector<ValidationError> &errors) {
            if (!expression) {
                errors.push_back({SeriesConfigValidationCode::MissingExpressionInput, path});
                return;
            }
            ++nodeCount;
            if (depth > kMaximumExpressionDepth) {
                errors.push_back({SeriesConfigValidationCode::ExpressionDepthLimit, path});
                return;
            }
            if (nodeCount > kMaximumExpressionNodes) {
                errors.push_back({SeriesConfigValidationCode::ExpressionNodeLimit, path});
                return;
            }

            const auto validateInput = [&](const Expression &input, const std::string_view field) {
                validateExpression(input, path + "." + std::string(field), depth + 1, nodeCount, errors);
            };
            std::visit([&](const auto &node) {
                using Node = std::decay_t<decltype(node)>;
                if constexpr (std::same_as<Node, PrimitiveReference>) {
                    if (!isKnownPrimitiveMetric(node.metric)) {
                        errors.push_back({SeriesConfigValidationCode::InvalidPrimitiveMetric, path + ".metric"});
                    }
                } else if constexpr (std::same_as<Node, NumericConstant>) {
                    if (!std::isfinite(node.value)) {
                        errors.push_back({SeriesConfigValidationCode::NonFiniteConstant, path + ".value"});
                    }
                } else if constexpr (std::same_as<Node, Add> || std::same_as<Node, Subtract> ||
                                     std::same_as<Node, Multiply> || std::same_as<Node, Divide>) {
                    validateInput(node.left, "left");
                    validateInput(node.right, "right");
                } else if constexpr (std::same_as<Node, RunningSum> || std::same_as<Node, ProjectedFinalValue> ||
                                     std::same_as<Node, ProjectRateToFinal>) {
                    validateInput(node.input, "input");
                } else if constexpr (std::same_as<Node, RollingMean>) {
                    if (node.window == 0) {
                        errors.push_back({SeriesConfigValidationCode::InvalidRollingWindow, path + ".window"});
                    }
                    validateInput(node.input, "input");
                } else if constexpr (std::same_as<Node, AverageAcrossRuns>) {
                    std::visit([&](const auto &selection) {
                        using Selection = std::decay_t<decltype(selection)>;
                        if constexpr (std::same_as<Selection, RecentRuns>) {
                            if (selection.count == 0) {
                                errors.push_back({
                                    SeriesConfigValidationCode::InvalidRecentRunCount, path + ".selection.count"
                                });
                            }
                        } else if (!std::isfinite(selection.percent) || selection.percent <= 0.0 || selection.percent >
                                   100.0) {
                            errors.push_back({
                                SeriesConfigValidationCode::InvalidTopPercentile, path + ".selection.percent"
                            });
                        }
                    }, node.selection);
                    validateInput(node.input, "input");
                }
            }, expression->value());
        }

        std::vector<ValidationError> validateConfig(const SeriesConfig &config, const std::string_view root) {
            std::vector<ValidationError> errors;
            std::visit([&](const auto &series) {
                using Config = std::decay_t<decltype(series)>;
                validatePresentation(series.presentation, std::same_as<Config, ComputedSeriesConfig>,
                                     std::string(root) + ".presentation", errors);
                if constexpr (std::same_as<Config, BaseSeriesConfig>) {
                    if (!isKnownPrimitiveMetric(series.metric)) {
                        errors.push_back({
                            SeriesConfigValidationCode::InvalidPrimitiveMetric, std::string(root) + ".metric"
                        });
                    }
                } else {
                    if (series.id.value == 0) {
                        errors.push_back({
                            SeriesConfigValidationCode::InvalidComputedSeriesId, std::string(root) + ".id"
                        });
                    }
                    size_t nodeCount = 0;
                    validateExpression(series.expression, std::string(root) + ".expression", 1, nodeCount, errors);
                }
            }, config);
            return errors;
        }

        [[nodiscard]] Expression makeExpression(ExpressionNode::Value value) {
            return std::make_shared<const ExpressionNode>(std::move(value));
        }
    }

    Expression primitive(const PrimitiveMetric metric) { return makeExpression(PrimitiveReference{metric}); }
    Expression numericConstant(const double value) { return makeExpression(NumericConstant{value}); }
    Expression add(Expression left, Expression right) { return makeExpression(Add{std::move(left), std::move(right)}); }

    Expression subtract(Expression left, Expression right) {
        return makeExpression(Subtract{std::move(left), std::move(right)});
    }

    Expression multiply(Expression left, Expression right) {
        return makeExpression(Multiply{std::move(left), std::move(right)});
    }

    Expression divide(Expression left, Expression right) {
        return makeExpression(Divide{std::move(left), std::move(right)});
    }

    Expression runningSum(Expression input) { return makeExpression(RunningSum{std::move(input)}); }

    Expression rollingMean(Expression input, const uint32_t window) {
        return makeExpression(RollingMean{std::move(input), window});
    }

    Expression projectedFinalValue(Expression input) { return makeExpression(ProjectedFinalValue{std::move(input)}); }
    Expression projectRateToFinal(Expression input) { return makeExpression(ProjectRateToFinal{std::move(input)}); }

    Expression averageAcrossRuns(Expression input, RunSelection selection) {
        return makeExpression(AverageAcrossRuns{std::move(input), std::move(selection)});
    }

    const SeriesPresentation &seriesPresentation(const SeriesConfig &config) {
        return std::visit([](const auto &series) -> const SeriesPresentation & { return series.presentation; }, config);
    }

    std::vector<ValidationError> validateSeriesConfig(const SeriesConfig &config) {
        return validateConfig(config, "record");
    }

    std::vector<ValidationError> validateSeriesConfig(const ComputedSeriesConfig &config) {
        return validateConfig(SeriesConfig{config}, "record");
    }

    std::vector<ValidationError> validateSeriesConfigs(const std::vector<SeriesConfig> &configs) {
        std::vector<ValidationError> errors;
        std::array<size_t, kPrimitiveMetrics.size()> baseCounts{};
        std::vector<std::pair<uint64_t, size_t> > computedIds;
        std::vector<std::pair<uint32_t, size_t> > positions;

        for (size_t index = 0; index < configs.size(); ++index) {
            const auto root = "records[" + std::to_string(index) + "]";
            auto recordErrors = validateConfig(configs[index], root);
            errors.insert(errors.end(), std::make_move_iterator(recordErrors.begin()),
                          std::make_move_iterator(recordErrors.end()));

            std::visit([&](const auto &series) {
                using Config = std::decay_t<decltype(series)>;
                positions.emplace_back(series.presentation.displayPosition, index);
                if constexpr (std::same_as<Config, BaseSeriesConfig>) {
                    const auto metric = std::ranges::find(kPrimitiveMetrics, series.metric);
                    if (metric != kPrimitiveMetrics.end()) {
                        const auto metricIndex = static_cast<size_t>(metric - kPrimitiveMetrics.begin());
                        if (++baseCounts[metricIndex] > 1) {
                            errors.push_back({SeriesConfigValidationCode::DuplicateBaseMetric, root + ".metric"});
                        }
                        if (series.presentation.name != canonicalName(series.metric)) {
                            errors.push_back({
                                SeriesConfigValidationCode::NonCanonicalBaseName, root + ".presentation.name"
                            });
                        }
                    }
                } else if (series.id.value != 0) {
                    computedIds.emplace_back(series.id.value, index);
                }
            }, configs[index]);
        }

        for (size_t index = 0; index < baseCounts.size(); ++index) {
            if (baseCounts[index] == 0) {
                errors.push_back({
                    SeriesConfigValidationCode::MissingBaseMetric,
                    "records.base[" + std::to_string(index) + "]"
                });
            }
        }

        std::ranges::sort(computedIds);
        for (size_t index = 1; index < computedIds.size(); ++index) {
            if (computedIds[index].first == computedIds[index - 1].first) {
                errors.push_back({
                    SeriesConfigValidationCode::DuplicateComputedSeriesId,
                    "records[" + std::to_string(computedIds[index].second) + "].id"
                });
            }
        }

        std::ranges::sort(positions);
        for (size_t index = 0; index < positions.size(); ++index) {
            if (index > 0 && positions[index].first == positions[index - 1].first) {
                errors.push_back({
                    SeriesConfigValidationCode::DuplicateDisplayPosition,
                    "records[" + std::to_string(positions[index].second) + "].presentation.displayPosition"
                });
            }
        }
        for (size_t index = 0; index < positions.size(); ++index) {
            if (positions[index].first != index) {
                errors.push_back({
                    SeriesConfigValidationCode::NonDenseDisplayPosition,
                    "records[" + std::to_string(positions[index].second) + "].presentation.displayPosition"
                });
                break;
            }
        }
        return errors;
    }

    std::vector<SeriesConfig> defaultSeriesConfigs() {
        const auto line = [](const RgbaColor color) { return LineStyle{color, 2.0}; };
        return {
            BaseSeriesConfig{PrimitiveMetric::Score, {"Score", line({0, 150, 0, 255}), true, 0}},
            ComputedSeriesConfig{
                ComputedSeriesId{1}, {"Accuracy", line({0, 255, 255, 255}), true, 1},
                divide(primitive(PrimitiveMetric::Hits), primitive(PrimitiveMetric::Shots))
            },
            BaseSeriesConfig{PrimitiveMetric::Shots, {"Shots", line({255, 165, 0, 255}), true, 2}},
            BaseSeriesConfig{PrimitiveMetric::Hits, {"Hits", line({100, 149, 237, 255}), false, 3}},
            BaseSeriesConfig{PrimitiveMetric::Kills, {"Kills", line({255, 0, 0, 255}), true, 4}},
            BaseSeriesConfig{PrimitiveMetric::Dmg, {"Dmg", line({255, 255, 0, 255}), true, 5}},
            ComputedSeriesConfig{
                ComputedSeriesId{2}, {"Score Total", line({128, 0, 128, 255}), true, 6},
                runningSum(primitive(PrimitiveMetric::Score))
            },
            ComputedSeriesConfig{
                ComputedSeriesId{3}, {"Expected Final Score", line({255, 0, 255, 255}), true, 7},
                projectedFinalValue(runningSum(primitive(PrimitiveMetric::Score)))
            },
            ComputedSeriesConfig{
                ComputedSeriesId{4}, {"Expected Final Score (5s)", line({0, 191, 255, 255}), true, 8},
                projectRateToFinal(rollingMean(primitive(PrimitiveMetric::Score), 5))
            }
        };
    }
}
