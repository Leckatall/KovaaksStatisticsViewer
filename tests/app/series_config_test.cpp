#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string_view>
#include <variant>

#include "app/contracts/expression_dsl.h"
#include "app/contracts/series_config.h"

using namespace ksv::application;

namespace {
    bool hasError(const std::vector<ValidationError> &errors, const SeriesConfigValidationCode code,
                  const std::string_view path = {}) {
        return std::ranges::any_of(errors, [code, path](const ValidationError &error) {
            return error.code == code && (path.empty() || error.path == path);
        });
    }

    TEST(SeriesConfigTest, SeriesConfigDefaultsToNoAxisAndIdentityTransform) {
        const SeriesConfig config{SeriesId{1}, {"Score", {}, true, 0}, primitive(PrimitiveMetric::Score)};

        EXPECT_FALSE(config.yAxisId.has_value());
        EXPECT_EQ(config.transformKind, AxisTransformKind::Identity);
    }

    TEST(SeriesConfigTest, AxisConfigHoldsIdNameOptionsAndTransformKind) {
        const AxisConfig axis{
            AxisId{5}, "Custom axis", {ValueAxisOptions::Baseline::Zero, true, 6, 2.0},
            AxisTransformKind::Percentage
        };

        EXPECT_EQ(axis.id.value, 5U);
        EXPECT_EQ(axis.name, "Custom axis");
        EXPECT_EQ(axis.options.baseline, ValueAxisOptions::Baseline::Zero);
        EXPECT_TRUE(axis.options.integral);
        EXPECT_EQ(axis.options.targetTicks, 6);
        EXPECT_DOUBLE_EQ(axis.options.fallbackSpan, 2.0);
        EXPECT_EQ(axis.transformKind, AxisTransformKind::Percentage);
    }

    TEST(SeriesConfigTest, DefaultLinesMatchTheApprovedTable) {
        struct Row {
            uint64_t id;
            const char *name;
            const char *dsl;
            std::optional<uint64_t> axis;
            AxisTransformKind transform;
            bool enabled;
            uint32_t hiddenLeading;
            RgbaColor color;
        };
        const std::array<Row, 8> expected{{
            {1, "Score", "RunningSum(SCORE)", 1, AxisTransformKind::Identity, true, 0, {224, 224, 224, 255}},
            {2, "Expected Final Score", "ProjectRateToFinal(RollingMean(window: 5, SCORE))", 1,
                AxisTransformKind::Identity, true, 1, {167, 139, 250, 255}},
            {3, "Expected Final (Top 10%)",
                "AverageAcrossRuns(over: top 10%, ProjectRateToFinal(RollingMean(window: 5, SCORE)))", 1,
                AxisTransformKind::Identity, true, 1, {221, 214, 254, 255}},
            {4, "Expected Final (Last 10 Runs)",
                "AverageAcrossRuns(over: recent 10, ProjectRateToFinal(RollingMean(window: 5, SCORE)))", 1,
                AxisTransformKind::Identity, true, 1, {139, 123, 196, 255}},
            {5, "Accuracy", "Divide(RunningSum(HITS), RunningSum(SHOTS))", 2, AxisTransformKind::Percentage, true, 0,
                {46, 196, 160, 255}},
            {6, "Accuracy (5s)", "Divide(RollingMean(window: 5, HITS), RollingMean(window: 5, SHOTS))", 2,
                AxisTransformKind::Percentage, false, 0, {31, 138, 112, 255}},
            {7, "Score/s (5s)", "RollingMean(window: 5, SCORE)", std::nullopt, AxisTransformKind::Identity, false, 0,
                {158, 158, 158, 255}},
            {8, "Expected Final (Whole Run)", "ProjectedFinalValue(RunningSum(SCORE))", 1,
                AxisTransformKind::Identity, false, 1, {244, 143, 177, 255}}
        }};

        const auto configs = defaultSeriesConfigs();
        ASSERT_EQ(configs.size(), expected.size());
        for (size_t index = 0; index < configs.size(); ++index) {
            const auto &config = configs[index];
            const auto &row = expected[index];
            SCOPED_TRACE(row.name);
            EXPECT_EQ(config.id.value, row.id);
            EXPECT_EQ(config.presentation.displayPosition, index);
            EXPECT_EQ(config.presentation.name, row.name);
            EXPECT_EQ(encodeExpressionDsl(config.expression), row.dsl);
            ASSERT_EQ(config.yAxisId.has_value(), row.axis.has_value());
            if (row.axis) EXPECT_EQ(config.yAxisId->value, *row.axis);
            EXPECT_EQ(config.transformKind, row.transform);
            EXPECT_EQ(config.presentation.enabled, row.enabled);
            EXPECT_EQ(config.presentation.hiddenLeadingPoints, row.hiddenLeading);
            EXPECT_EQ(config.presentation.lineStyle.color, row.color);
            EXPECT_DOUBLE_EQ(config.presentation.lineStyle.width, 3.0);
        }
        EXPECT_TRUE(validateSeriesConfigs(configs).empty());
    }

    TEST(SeriesConfigTest, DefaultAxesAreScoreAndAccuracyWithFixedIds) {
        const auto axes = defaultAxisConfigs();

        ASSERT_EQ(axes.size(), 2U);
        EXPECT_EQ(axes[0].id.value, 1U);
        EXPECT_EQ(axes[0].name, "Score");
        EXPECT_EQ(axes[0].transformKind, AxisTransformKind::Identity);
        EXPECT_EQ(axes[1].id.value, 2U);
        EXPECT_EQ(axes[1].name, "Accuracy");
        EXPECT_EQ(axes[1].transformKind, AxisTransformKind::Percentage);
        for (const auto &axis : axes) {
            EXPECT_EQ(axis.options.baseline, ValueAxisOptions::Baseline::HugData);
            EXPECT_FALSE(axis.options.integral);
            EXPECT_EQ(axis.options.targetTicks, 10);
        }
    }

    TEST(SeriesConfigTest, ProjectRateToFinalParticipatesInValidationLimits) {
        const SeriesConfig missing{SeriesId{9}, {"Missing", {}, true, 0}, projectRateToFinal({})};
        const auto missingErrors = validateSeriesConfigs({missing});
        EXPECT_TRUE(hasError(missingErrors, SeriesConfigValidationCode::MissingExpressionInput,
                             "records[0].expression.input"));

        Expression deep = primitive(PrimitiveMetric::Score);
        for (size_t index = 0; index < 16; ++index) deep = projectRateToFinal(deep);
        const auto deepErrors = validateSeriesConfigs({SeriesConfig{SeriesId{10}, {"Deep", {}, true, 0}, deep}});
        EXPECT_TRUE(hasError(deepErrors, SeriesConfigValidationCode::ExpressionDepthLimit));
    }

    TEST(SeriesConfigTest, ValidationReportsStableCodesAndFieldPaths) {
        auto configs = defaultSeriesConfigs();
        configs[1].id = SeriesId{0};
        configs[1].presentation.name = " ";
        configs[1].presentation.lineStyle.width = std::numeric_limits<double>::infinity();
        configs[1].expression = runningSum({});

        const auto errors = validateSeriesConfigs(configs);
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::InvalidComputedSeriesId, "records[1].id"));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::EmptyComputedName,
                             "records[1].presentation.name"));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::NonFiniteLineWidth,
                             "records[1].presentation.lineStyle.width"));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::MissingExpressionInput,
                             "records[1].expression.input"));
    }

    TEST(SeriesConfigTest, HiddenLeadingPointsAreBoundedByTheMaximum) {
        auto configs = defaultSeriesConfigs();
        configs[1].presentation.hiddenLeadingPoints = 0;
        EXPECT_TRUE(validateSeriesConfigs(configs).empty());
        configs[1].presentation.hiddenLeadingPoints = kMaximumHiddenLeadingPoints;
        EXPECT_TRUE(validateSeriesConfigs(configs).empty());

        configs[1].presentation.hiddenLeadingPoints = kMaximumHiddenLeadingPoints + 1;
        EXPECT_TRUE(hasError(validateSeriesConfigs(configs), SeriesConfigValidationCode::HiddenLeadingPointsOutOfRange,
                             "records[1].presentation.hiddenLeadingPoints"));
    }

    TEST(SeriesConfigTest, SameSeriesConfigsNoticesAHiddenLeadingPointsDifference) {
        const auto left = defaultSeriesConfigs();
        auto right = left;
        EXPECT_TRUE(sameSeriesConfigs(left, right));
        right[2].presentation.hiddenLeadingPoints = 3;
        EXPECT_FALSE(sameSeriesConfigs(left, right));
    }

    TEST(SeriesConfigTest, BlankTopLevelExpressionIsAllowedButNestedMissingOperandIsNot) {
        auto configs = defaultSeriesConfigs();
        configs[0].expression = {};
        EXPECT_TRUE(validateSeriesConfigs(configs).empty());

        configs[1].expression = add({}, primitive(PrimitiveMetric::Score));
        const auto errors = validateSeriesConfigs(configs);
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::MissingExpressionInput,
                             "records[1].expression.left"));
    }

    TEST(SeriesConfigTest, ValidationRejectsInvalidExpressionParametersAndCollectionIntegrity) {
        auto configs = defaultSeriesConfigs();
        configs[1].expression = rollingMean(primitive(PrimitiveMetric::Score), 0);
        configs[0].presentation.name = "Renamed";
        configs[2].expression = primitive(PrimitiveMetric::Score);
        configs[6].id = SeriesId{1};
        configs[3].presentation.displayPosition = 7;

        const auto errors = validateSeriesConfigs(configs);
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::InvalidRollingWindow));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::DuplicateComputedSeriesId));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::DuplicateDisplayPosition));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::NonDenseDisplayPosition));
    }

    TEST(SeriesConfigTest, DuplicatePrimitiveReferencesAreNoLongerRejected) {
        auto configs = defaultSeriesConfigs();
        configs[2].presentation.name = "Kills Again";
        configs[2].expression = primitive(PrimitiveMetric::Kills);

        EXPECT_TRUE(validateSeriesConfigs(configs).empty());
    }

    TEST(SeriesConfigTest, RenamingALineToAnEmptyOrUntrimmedNameIsStillRejected) {
        auto configs = defaultSeriesConfigs();
        configs[0].presentation.name = "  ";

        const auto errors = validateSeriesConfigs(configs);
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::EmptyComputedName));
    }

    TEST(SeriesConfigTest, CopiedExpressionsContainOnlyPrimitiveLeaves) {
        const auto source = divide(primitive(PrimitiveMetric::Hits), primitive(PrimitiveMetric::Shots));
        const auto copy = source;
        const SeriesConfig config{SeriesId{99}, {"Copy", {}, true, 0}, copy};

        ASSERT_TRUE(validateSeriesConfigs({config}).empty());
        const auto &expression = std::get<Divide>(config.expression->value());
        EXPECT_EQ(std::get<PrimitiveReference>(expression.left->value()).metric, PrimitiveMetric::Hits);
        EXPECT_EQ(std::get<PrimitiveReference>(expression.right->value()).metric, PrimitiveMetric::Shots);
    }

    TEST(SeriesConfigTest, IndividualValidationCoversNamesStylesAndExpressionParameters) {
        const SeriesPresentation presentation{" name ", {{}, 0.25}, true, 0};
        const SeriesConfig config{
            SeriesId{9}, presentation,
            averageAcrossRuns(numericConstant(std::numeric_limits<double>::quiet_NaN()), TopPercentile{101.0})
        };

        const auto errors = validateSeriesConfigs({config});
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::ComputedNameNotTrimmed));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::LineWidthOutOfRange));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::NonFiniteConstant));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::InvalidTopPercentile));

        const SeriesConfig recent{
            SeriesId{10}, {std::string(121, 'x'), {}, true, 1},
            averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{0})
        };
        const auto recentErrors = validateSeriesConfigs({recent});
        EXPECT_TRUE(hasError(recentErrors, SeriesConfigValidationCode::ComputedNameTooLong));
        EXPECT_TRUE(hasError(recentErrors, SeriesConfigValidationCode::InvalidRecentRunCount));
    }

    TEST(SeriesConfigTest, IndividualValidationEnforcesExpressionDepthAndNodeLimits) {
        Expression deep = primitive(PrimitiveMetric::Score);
        for (size_t index = 0; index < 16; ++index) deep = runningSum(deep);
        const auto deepErrors = validateSeriesConfigs({SeriesConfig{SeriesId{9}, {"Deep", {}, true, 0}, deep}});
        EXPECT_TRUE(hasError(deepErrors, SeriesConfigValidationCode::ExpressionDepthLimit));

        const std::function<Expression(size_t)> completeTree = [&](const size_t levels) -> Expression {
            if (levels == 0) return primitive(PrimitiveMetric::Score);
            return add(completeTree(levels - 1), completeTree(levels - 1));
        };
        const Expression large = completeTree(8);
        const auto largeErrors = validateSeriesConfigs({SeriesConfig{SeriesId{10}, {"Large", {}, true, 0}, large}});
        EXPECT_TRUE(hasError(largeErrors, SeriesConfigValidationCode::ExpressionNodeLimit));
    }
}
