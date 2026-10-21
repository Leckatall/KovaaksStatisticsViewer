#include "series_config.h"

#include "expression_dsl.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <ranges>
#include <string_view>
#include <tuple>
#include <utility>

namespace ksv::application {
    namespace {
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

        void validatePresentation(const SeriesPresentation &presentation,
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

            if (presentation.hiddenLeadingPoints > kMaximumHiddenLeadingPoints) {
                errors.push_back({
                    SeriesConfigValidationCode::HiddenLeadingPointsOutOfRange,
                    std::string(path) + ".hiddenLeadingPoints"
                });
            }

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
                                size_t &node_count, std::vector<ValidationError> &errors) {
            if (!expression) {
                errors.push_back({SeriesConfigValidationCode::MissingExpressionInput, path});
                return;
            }
            ++node_count;
            if (depth > kMaximumExpressionDepth) {
                errors.push_back({SeriesConfigValidationCode::ExpressionDepthLimit, path});
                return;
            }
            if (node_count > kMaximumExpressionNodes) {
                errors.push_back({SeriesConfigValidationCode::ExpressionNodeLimit, path});
                return;
            }

            const auto validate_input = [&](const Expression &input, const std::string_view field) {
                validateExpression(input, path + "." + std::string(field), depth + 1, node_count, errors);
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
                    validate_input(node.left, "left");
                    validate_input(node.right, "right");
                } else if constexpr (std::same_as<Node, RunningSum> || std::same_as<Node, ProjectedFinalValue> ||
                                     std::same_as<Node, ProjectRateToFinal>) {
                    validate_input(node.input, "input");
                } else if constexpr (std::same_as<Node, RollingMean>) {
                    if (node.window == 0) {
                        errors.push_back({SeriesConfigValidationCode::InvalidRollingWindow, path + ".window"});
                    }
                    validate_input(node.input, "input");
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
                    validate_input(node.input, "input");
                }
            }, expression->value());
        }

        std::vector<ValidationError> validateConfig(const SeriesConfig &config, const std::string_view root) {
            std::vector<ValidationError> errors;
            validatePresentation(config.presentation, std::string(root) + ".presentation", errors);
            if (config.id.value == 0) {
                errors.push_back({SeriesConfigValidationCode::InvalidComputedSeriesId, std::string(root) + ".id"});
            }
            // A null top-level expression is a deliberately blank series (plots nothing). A null
            // operand *inside* an expression is still a missing input — validateExpression keeps
            // reporting that via its recursive validate_input calls.
            if (config.expression) {
                size_t node_count = 0;
                validateExpression(config.expression, std::string(root) + ".expression", 1, node_count, errors);
            }
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

    Expression averageAcrossRuns(Expression input, const RunSelection selection) {
        return makeExpression(AverageAcrossRuns{std::move(input), selection});
    }

    std::vector<ValidationError> validateSeriesConfigs(const std::vector<SeriesConfig> &configs) {
        std::vector<ValidationError> errors;
        std::vector<std::pair<uint64_t, size_t> > series_ids;
        std::vector<std::pair<uint32_t, size_t> > positions;

        for (size_t index = 0; index < configs.size(); ++index) {
            const auto root = "records[" + std::to_string(index) + "]";
            const SeriesConfig &config = configs[index];
            auto record_errors = validateConfig(config, root);
            errors.insert(errors.end(), std::make_move_iterator(record_errors.begin()),
                          std::make_move_iterator(record_errors.end()));

            positions.emplace_back(config.presentation.displayPosition, index);
            if (config.id.value != 0) series_ids.emplace_back(config.id.value, index);
        }

        std::ranges::sort(series_ids);
        if (auto it = std::ranges::adjacent_find(series_ids, [](const auto &left, const auto &right) {
            return left.first == right.first;
        }); it != series_ids.end()) {
            errors.push_back({
                SeriesConfigValidationCode::DuplicateComputedSeriesId,
                "records[" + std::to_string(it->second) + "].id"
            });
        }
        std::ranges::sort(positions);
        for (size_t index = 0; index < positions.size(); ++index) {
            if (index > 0 && positions[index].first == positions[index - 1].first) {
                errors.push_back({
                    SeriesConfigValidationCode::DuplicateDisplayPosition,
                    "records[" + std::to_string(positions[index].second) + "].presentation.displayPosition"
                });
            }
            if (positions[index].first != index) {
                errors.push_back({
                    SeriesConfigValidationCode::NonDenseDisplayPosition,
                    "records[" + std::to_string(positions[index].second) + "].presentation.displayPosition"
                });
            }
        }
        return errors;
    }

    bool sameSeriesConfigs(const std::vector<SeriesConfig> &left, const std::vector<SeriesConfig> &right) {
        if (left.size() != right.size()) return false;
        for (size_t index = 0; index < left.size(); ++index) {
            const auto &[leftId, leftPresentation, leftExpression, leftAxis, leftTransform] = left[index];
            const auto &[rightId, rightPresentation, rightExpression, rightAxis, rightTransform] = right[index];
            if (leftId != rightId || leftPresentation.name != rightPresentation.name ||
                leftPresentation.lineStyle.color != rightPresentation.lineStyle.color ||
                leftPresentation.lineStyle.width != rightPresentation.lineStyle.width ||
                leftPresentation.enabled != rightPresentation.enabled ||
                leftPresentation.displayPosition != rightPresentation.displayPosition ||
                leftPresentation.hiddenLeadingPoints != rightPresentation.hiddenLeadingPoints || leftAxis != rightAxis
                ||
                leftTransform != rightTransform || encodeExpressionDsl(leftExpression) != encodeExpressionDsl(
                    rightExpression))
                return false;
        }
        return true;
    }

    bool sameAxisConfigs(const std::vector<AxisConfig> &left, const std::vector<AxisConfig> &right) {
        if (left.size() != right.size()) return false;
        for (size_t index = 0; index < left.size(); ++index) {
            const auto &[leftOptions, rightOptions] = std::tie(left[index].options, right[index].options);
            if (left[index].id != right[index].id || left[index].name != right[index].name ||
                leftOptions.baseline != rightOptions.baseline || leftOptions.integral != rightOptions.integral ||
                leftOptions.targetTicks != rightOptions.targetTicks ||
                leftOptions.fallbackSpan != rightOptions.fallbackSpan ||
                left[index].transformKind != right[index].transformKind)
                return false;
        }
        return true;
    }

    std::vector<SeriesConfig> defaultSeriesConfigs() {
        const auto line = [](const RgbaColor color) { return LineStyle{color, 3.0}; };
        const auto score = primitive(PrimitiveMetric::Score);
        const auto expectedFinalScore = [&] { return projectRateToFinal(rollingMean(score, 5)); };
        const auto accuracyOver5s = divide(rollingMean(primitive(PrimitiveMetric::Hits), 5),
                                           rollingMean(primitive(PrimitiveMetric::Shots), 5));
        return {
            SeriesConfig{
                SeriesId{1}, {"Score", line({224, 224, 224, 255}), true, 0},
                runningSum(score), AxisId{1}
            },
            SeriesConfig{
                SeriesId{2}, {"Expected Final Score", line({167, 139, 250, 255}), true, 1, 1},
                expectedFinalScore(), AxisId{1}
            },
            SeriesConfig{
                SeriesId{3}, {"Expected Final (Top 10%)", line({221, 214, 254, 255}), true, 2, 1},
                averageAcrossRuns(expectedFinalScore(), TopPercentile{10.0}), AxisId{1}
            },
            SeriesConfig{
                SeriesId{4}, {"Expected Final (Last 10 Runs)", line({139, 123, 196, 255}), true, 3, 1},
                averageAcrossRuns(expectedFinalScore(), RecentRuns{10}), AxisId{1}
            },
            SeriesConfig{
                SeriesId{5}, {"Accuracy", line({46, 196, 160, 255}), true, 4},
                divide(runningSum(primitive(PrimitiveMetric::Hits)), runningSum(primitive(PrimitiveMetric::Shots))),
                AxisId{2}, AxisTransformKind::Percentage
            },
            SeriesConfig{
                SeriesId{6}, {"Accuracy (5s)", line({31, 138, 112, 255}), false, 5},
                accuracyOver5s, AxisId{2}, AxisTransformKind::Percentage
            },
            SeriesConfig{
                SeriesId{7}, {"Score/s (5s)", line({158, 158, 158, 255}), false, 6},
                rollingMean(score, 5)
            },
            SeriesConfig{
                SeriesId{8}, {"Expected Final (Whole Run)", line({244, 143, 177, 255}), false, 7, 1},
                projectedFinalValue(runningSum(score)), AxisId{1}
            }
        };
    }

    std::vector<AxisConfig> defaultAxisConfigs() {
        return {
            AxisConfig{AxisId{1}, "Score", {}, AxisTransformKind::Identity},
            AxisConfig{AxisId{2}, "Accuracy", {}, AxisTransformKind::Percentage}
        };
    }
}
