#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ksv::application {
    enum class PrimitiveMetric {
        Score,
        Shots,
        Hits,
        Kills,
        Dmg
    };

    inline constexpr std::array kPrimitiveMetrics{
        PrimitiveMetric::Score,
        PrimitiveMetric::Shots,
        PrimitiveMetric::Hits,
        PrimitiveMetric::Kills,
        PrimitiveMetric::Dmg
    };

    struct SeriesId {
        uint64_t value = 0;

        auto operator<=>(const SeriesId &) const = default;
    };

    inline constexpr SeriesId kFirstUserComputedSeriesId{10};

    struct RgbaColor {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        uint8_t alpha = 255;

        auto operator<=>(const RgbaColor &) const = default;
    };

    struct LineStyle {
        RgbaColor color{};
        double width = 2.0;
    };

    struct SeriesPresentation {
        std::string name;
        LineStyle lineStyle{};
        bool enabled = true;
        uint32_t displayPosition = 0;
    };

    struct RecentRuns {
        uint32_t count = 0;
    };

    struct TopPercentile {
        double percent = 0.0;
    };

    using RunSelection = std::variant<RecentRuns, TopPercentile>;

    class ExpressionNode;
    using Expression = std::shared_ptr<const ExpressionNode>;

    struct PrimitiveReference {
        PrimitiveMetric metric;
    };

    struct NumericConstant {
        double value;
    };

    struct Add {
        Expression left;
        Expression right;
    };

    struct Subtract {
        Expression left;
        Expression right;
    };

    struct Multiply {
        Expression left;
        Expression right;
    };

    struct Divide {
        Expression left;
        Expression right;
    };

    struct RunningSum {
        Expression input;
    };

    struct RollingMean {
        Expression input;
        uint32_t window;
    };

    struct ProjectedFinalValue {
        Expression input;
    };

    struct ProjectRateToFinal {
        Expression input;
    };

    struct AverageAcrossRuns {
        Expression input;
        RunSelection selection;
    };

    class ExpressionNode {
    public:
        using Value = std::variant<PrimitiveReference, NumericConstant, Add, Subtract, Multiply, Divide,
            RunningSum, RollingMean, ProjectedFinalValue, ProjectRateToFinal, AverageAcrossRuns>;

        explicit ExpressionNode(Value value) : m_value(std::move(value)) {
        }

        [[nodiscard]] const Value &value() const { return m_value; }

    private:
        Value m_value;
    };

    [[nodiscard]] Expression primitive(PrimitiveMetric metric);

    [[nodiscard]] Expression numericConstant(double value);

    [[nodiscard]] Expression add(Expression left, Expression right);

    [[nodiscard]] Expression subtract(Expression left, Expression right);

    [[nodiscard]] Expression multiply(Expression left, Expression right);

    [[nodiscard]] Expression divide(Expression left, Expression right);

    [[nodiscard]] Expression runningSum(Expression input);

    [[nodiscard]] Expression rollingMean(Expression input, uint32_t window);

    [[nodiscard]] Expression projectedFinalValue(Expression input);

    [[nodiscard]] Expression projectRateToFinal(Expression input);

    [[nodiscard]] Expression averageAcrossRuns(Expression input, RunSelection selection);

    struct SeriesConfig {
        SeriesId id;
        SeriesPresentation presentation;
        Expression expression;

        [[nodiscard]] bool isPrimitive() const {
            return expression && std::holds_alternative<PrimitiveReference>(expression->value());
        }
    };

    enum class SeriesConfigValidationCode {
        InvalidPrimitiveMetric,
        InvalidComputedSeriesId,
        EmptyComputedName,
        ComputedNameNotTrimmed,
        ComputedNameTooLong,
        NonFiniteLineWidth,
        LineWidthOutOfRange,
        MissingExpressionInput,
        NonFiniteConstant,
        InvalidRollingWindow,
        InvalidRecentRunCount,
        InvalidTopPercentile,
        ExpressionDepthLimit,
        ExpressionNodeLimit,
        DuplicateComputedSeriesId,
        DuplicateDisplayPosition,
        NonDenseDisplayPosition
    };

    struct ValidationError {
        SeriesConfigValidationCode code;
        std::string path;
    };

    class SeriesConfigValidator {

    };

    [[nodiscard]] std::vector<ValidationError> validateSeriesConfig(const SeriesConfig &config);

    [[nodiscard]] std::vector<ValidationError> validateSeriesConfigs(const std::vector<SeriesConfig> &configs);

    [[nodiscard]] std::vector<SeriesConfig> defaultSeriesConfigs();
}
