#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <ranges>
#include <variant>

#include "app/contracts/series_config.h"

using namespace ksv::application;

namespace {
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
        const auto missingErrors = validateSeriesConfig(missing);
        EXPECT_TRUE(std::ranges::any_of(missingErrors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::MissingExpressionInput && error.path ==
                   "record.expression.input";
        }));

        Expression deep = primitive(PrimitiveMetric::Score);
        for (size_t index = 0; index < 16; ++index) deep = projectRateToFinal(deep);
        const auto deepErrors = validateSeriesConfig(SeriesConfig{SeriesId{10}, {"Deep", {}, true, 0}, deep});
        EXPECT_TRUE(std::ranges::any_of(deepErrors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::ExpressionDepthLimit;
        }));
    }

    TEST(SeriesConfigTest, ValidationReportsStableCodesAndFieldPaths) {
        auto configs = defaultSeriesConfigs();
        configs[1].id = SeriesId{0};
        configs[1].presentation.name = " ";
        configs[1].presentation.lineStyle.width = std::numeric_limits<double>::infinity();
        configs[1].expression = {};

        const auto errors = validateSeriesConfigs(configs);
        EXPECT_TRUE(std::ranges::any_of(errors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::InvalidComputedSeriesId && error.path == "records[1].id";
        }));
        EXPECT_TRUE(std::ranges::any_of(errors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::EmptyComputedName && error.path ==
                   "records[1].presentation.name";
        }));
        EXPECT_TRUE(std::ranges::any_of(errors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::NonFiniteLineWidth &&
                   error.path == "records[1].presentation.lineStyle.width";
        }));
        EXPECT_TRUE(std::ranges::any_of(errors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::MissingExpressionInput && error.path ==
                   "records[1].expression";
        }));
    }

    TEST(SeriesConfigTest, ValidationRejectsInvalidExpressionParametersAndCollectionIntegrity) {
        auto configs = defaultSeriesConfigs();
        configs[1].expression = rollingMean(primitive(PrimitiveMetric::Score), 0);
        configs[0].presentation.name = "Renamed";
        configs[2].expression = primitive(PrimitiveMetric::Score);
        configs[6].id = SeriesId{1};
        configs[3].presentation.displayPosition = 8;

        const auto errors = validateSeriesConfigs(configs);
        const auto hasCode = [&errors](const SeriesConfigValidationCode code) {
            return std::ranges::any_of(errors, [code](const ValidationError &error) { return error.code == code; });
        };
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::InvalidRollingWindow));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::NonCanonicalBaseName));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::DuplicateBaseMetric));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::MissingBaseMetric));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::DuplicateComputedSeriesId));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::DuplicateDisplayPosition));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::NonDenseDisplayPosition));
    }

    TEST(SeriesConfigTest, CopiedExpressionsContainOnlyPrimitiveLeaves) {
        const auto source = divide(primitive(PrimitiveMetric::Hits), primitive(PrimitiveMetric::Shots));
        const auto copy = source;
        const SeriesConfig config{SeriesId{99}, {"Copy", {}, true, 9}, copy};

        ASSERT_TRUE(validateSeriesConfig(config).empty());
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

        const auto errors = validateSeriesConfig(config);
        const auto hasCode = [&errors](const SeriesConfigValidationCode code) {
            return std::ranges::any_of(errors, [code](const ValidationError &error) { return error.code == code; });
        };
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::ComputedNameNotTrimmed));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::LineWidthOutOfRange));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::NonFiniteConstant));
        EXPECT_TRUE(hasCode(SeriesConfigValidationCode::InvalidTopPercentile));

        const SeriesConfig recent{
            SeriesId{10}, {std::string(121, 'x'), {}, true, 1},
            averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{0})
        };
        const auto recentErrors = validateSeriesConfig(recent);
        EXPECT_TRUE(std::ranges::any_of(recentErrors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::ComputedNameTooLong;
        }));
        EXPECT_TRUE(std::ranges::any_of(recentErrors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::InvalidRecentRunCount;
        }));
    }

    TEST(SeriesConfigTest, IndividualValidationEnforcesExpressionDepthAndNodeLimits) {
        Expression deep = primitive(PrimitiveMetric::Score);
        for (size_t index = 0; index < 16; ++index) deep = runningSum(deep);
        const auto deepErrors = validateSeriesConfig(SeriesConfig{SeriesId{9}, {"Deep", {}, true, 0}, deep});
        EXPECT_TRUE(std::ranges::any_of(deepErrors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::ExpressionDepthLimit;
        }));

        const std::function<Expression(size_t)> completeTree = [&](const size_t levels) -> Expression {
            if (levels == 0) return primitive(PrimitiveMetric::Score);
            return add(completeTree(levels - 1), completeTree(levels - 1));
        };
        const Expression large = completeTree(8);
        const auto largeErrors = validateSeriesConfig(SeriesConfig{SeriesId{10}, {"Large", {}, true, 0}, large});
        EXPECT_TRUE(std::ranges::any_of(largeErrors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::ExpressionNodeLimit;
        }));
    }
}
