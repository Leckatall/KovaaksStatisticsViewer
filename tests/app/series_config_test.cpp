#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <ranges>
#include <string_view>
#include <variant>

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
            AxisId{5}, "Custom axis", {AxisModelOptions::Baseline::Zero, true, 6, 2.0},
            AxisTransformKind::Percentage
        };

        EXPECT_EQ(axis.id.value, 5U);
        EXPECT_EQ(axis.name, "Custom axis");
        EXPECT_EQ(axis.options.baseline, AxisModelOptions::Baseline::Zero);
        EXPECT_TRUE(axis.options.integral);
        EXPECT_EQ(axis.options.targetTicks, 6);
        EXPECT_DOUBLE_EQ(axis.options.fallbackSpan, 2.0);
        EXPECT_EQ(axis.transformKind, AxisTransformKind::Percentage);
    }

    TEST(SeriesConfigTest, DefaultsContainTheCompleteCatalogueInDisplayOrder) {
        const auto configs = defaultSeriesConfigs();

        ASSERT_EQ(configs.size(), 9U);
        for (size_t position = 0; position < configs.size(); ++position)
            EXPECT_EQ(configs[position].presentation.displayPosition, position);

        const auto &score = configs[0];
        const auto &hits = configs[3];
        EXPECT_TRUE(score.isPrimitive());
        EXPECT_EQ(std::get<PrimitiveReference>(score.expression->value()).metric, PrimitiveMetric::Score);
        EXPECT_EQ(score.presentation.name, "Score");
        EXPECT_EQ(score.presentation.lineStyle.color, (RgbaColor{0, 150, 0, 255}));
        EXPECT_DOUBLE_EQ(score.presentation.lineStyle.width, 2.0);
        EXPECT_TRUE(hits.isPrimitive());
        EXPECT_EQ(std::get<PrimitiveReference>(hits.expression->value()).metric, PrimitiveMetric::Hits);
        EXPECT_FALSE(hits.presentation.enabled);
        EXPECT_EQ(hits.presentation.lineStyle.color, (RgbaColor{100, 149, 237, 255}));

        EXPECT_EQ(configs[1].id.value, 2U);
        EXPECT_EQ(configs[6].id.value, 7U);
        EXPECT_EQ(configs[7].id.value, 8U);
        EXPECT_EQ(configs[8].id.value, 9U);
        EXPECT_EQ(kFirstUserComputedSeriesId.value, 10U);

        constexpr std::array expectedNames{
            "Score", "Accuracy", "Shots", "Hits", "Kills", "Dmg", "Score Total", "Expected Final Score",
            "Expected Final Score (5s)"
        };
        constexpr std::array expectedColors{
            RgbaColor{0, 150, 0, 255}, RgbaColor{0, 255, 255, 255}, RgbaColor{255, 165, 0, 255},
            RgbaColor{100, 149, 237, 255}, RgbaColor{255, 0, 0, 255}, RgbaColor{255, 255, 0, 255},
            RgbaColor{128, 0, 128, 255}, RgbaColor{255, 0, 255, 255}, RgbaColor{0, 191, 255, 255}
        };
        for (size_t index = 0; index < configs.size(); ++index) {
            EXPECT_EQ(configs[index].presentation.name, expectedNames[index]);
            EXPECT_EQ(configs[index].presentation.lineStyle.color, expectedColors[index]);
            EXPECT_DOUBLE_EQ(configs[index].presentation.lineStyle.width, 2.0);
        }
    }

    TEST(SeriesConfigTest, DefaultAxesAreAccuracyAndScoreFamilyWithFixedIds) {
        const auto axes = defaultAxisConfigs();

        ASSERT_EQ(axes.size(), 2U);
        EXPECT_EQ(axes[0].id.value, 1U);
        EXPECT_EQ(axes[0].name, "Accuracy");
        EXPECT_EQ(axes[0].transformKind, AxisTransformKind::Percentage);
        EXPECT_EQ(axes[1].id.value, 2U);
        EXPECT_EQ(axes[1].name, "Score Family");
        EXPECT_EQ(axes[1].transformKind, AxisTransformKind::Identity);
        EXPECT_EQ(kFirstUserAxisId.value, 3U);
    }

    TEST(SeriesConfigTest, DefaultSeriesAssignScoreFamilyAxisAndAccuracyTransform) {
        const auto configs = defaultSeriesConfigs();
        const auto axes = defaultAxisConfigs();
        const auto scoreFamilyAxisId = axes[1].id;

        EXPECT_FALSE(configs[1].yAxisId.has_value());
        EXPECT_EQ(configs[1].transformKind, AxisTransformKind::Percentage);

        for (const size_t index: {6U, 7U, 8U}) {
            EXPECT_EQ(configs[index].id.value, index + 1);
            ASSERT_TRUE(configs[index].yAxisId.has_value());
            EXPECT_EQ(*configs[index].yAxisId, scoreFamilyAxisId);
            EXPECT_EQ(configs[index].transformKind, AxisTransformKind::Identity);
        }

        for (const size_t index: {0U, 2U, 3U, 4U, 5U}) {
            EXPECT_FALSE(configs[index].yAxisId.has_value());
            EXPECT_EQ(configs[index].transformKind, AxisTransformKind::Identity);
        }
    }

    TEST(SeriesConfigTest, DefaultProjectedScoreExpressionsPreserveLegacyIntent) {
        const auto configs = defaultSeriesConfigs();

        const auto &accuracy = std::get<Divide>(configs[1].expression->value());
        EXPECT_EQ(std::get<PrimitiveReference>(accuracy.left->value()).metric, PrimitiveMetric::Hits);
        EXPECT_EQ(std::get<PrimitiveReference>(accuracy.right->value()).metric, PrimitiveMetric::Shots);

        const auto &scoreTotal = std::get<RunningSum>(configs[6].expression->value());
        EXPECT_EQ(std::get<PrimitiveReference>(scoreTotal.input->value()).metric, PrimitiveMetric::Score);

        const auto &expected = std::get<ProjectedFinalValue>(configs[7].expression->value());
        const auto &total = std::get<RunningSum>(expected.input->value());
        EXPECT_EQ(std::get<PrimitiveReference>(total.input->value()).metric, PrimitiveMetric::Score);

        const auto &recent = std::get<ProjectRateToFinal>(configs[8].expression->value());
        const auto &mean = std::get<RollingMean>(recent.input->value());
        EXPECT_EQ(mean.window, 5U);
        EXPECT_EQ(std::get<PrimitiveReference>(mean.input->value()).metric, PrimitiveMetric::Score);
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
        configs[3].presentation.displayPosition = 8;

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

    TEST(SeriesConfigTest, RenamingAPrimitiveRowToAnEmptyOrUntrimmedNameIsStillRejected) {
        auto configs = defaultSeriesConfigs();
        ASSERT_TRUE(configs[0].isPrimitive());
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
