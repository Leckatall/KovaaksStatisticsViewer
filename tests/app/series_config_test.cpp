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
    const ComputedSeriesConfig &computedAt(const std::vector<SeriesConfig> &configs, const size_t position) {
        return std::get<ComputedSeriesConfig>(configs.at(position));
    }

    TEST(SeriesConfigTest, DefaultsContainTheCompleteCatalogueInDisplayOrder) {
        const auto configs = defaultSeriesConfigs();

        ASSERT_EQ(configs.size(), 9U);
        for (size_t position = 0; position < configs.size(); ++position) {
            EXPECT_EQ(seriesPresentation(configs[position]).displayPosition, position);
        }

        const auto &score = std::get<BaseSeriesConfig>(configs[0]);
        const auto &hits = std::get<BaseSeriesConfig>(configs[3]);
        EXPECT_EQ(score.metric, PrimitiveMetric::Score);
        EXPECT_EQ(score.presentation.name, "Score");
        EXPECT_EQ(score.presentation.lineStyle.color, (RgbaColor{0, 150, 0, 255}));
        EXPECT_DOUBLE_EQ(score.presentation.lineStyle.width, 2.0);
        EXPECT_EQ(hits.metric, PrimitiveMetric::Hits);
        EXPECT_FALSE(hits.presentation.enabled);
        EXPECT_EQ(hits.presentation.lineStyle.color, (RgbaColor{100, 149, 237, 255}));

        EXPECT_EQ(computedAt(configs, 1).id.value, 1U);
        EXPECT_EQ(computedAt(configs, 6).id.value, 2U);
        EXPECT_EQ(computedAt(configs, 7).id.value, 3U);
        EXPECT_EQ(computedAt(configs, 8).id.value, 4U);
        EXPECT_EQ(kFirstUserComputedSeriesId.value, 5U);

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
            const auto &presentation = seriesPresentation(configs[index]);
            EXPECT_EQ(presentation.name, expectedNames[index]);
            EXPECT_EQ(presentation.lineStyle.color, expectedColors[index]);
            EXPECT_DOUBLE_EQ(presentation.lineStyle.width, 2.0);
        }
    }

    TEST(SeriesConfigTest, DefaultExpressionsHaveTheApprovedStructure) {
        const auto configs = defaultSeriesConfigs();

        const auto &accuracy = std::get<Divide>(computedAt(configs, 1).expression->value());
        EXPECT_EQ(std::get<PrimitiveReference>(accuracy.left->value()).metric, PrimitiveMetric::Hits);
        EXPECT_EQ(std::get<PrimitiveReference>(accuracy.right->value()).metric, PrimitiveMetric::Shots);

        const auto &scoreTotal = std::get<RunningSum>(computedAt(configs, 6).expression->value());
        EXPECT_EQ(std::get<PrimitiveReference>(scoreTotal.input->value()).metric, PrimitiveMetric::Score);

        const auto &expected = std::get<ProjectedFinalValue>(computedAt(configs, 7).expression->value());
        EXPECT_EQ(std::get<PrimitiveReference>(expected.input->value()).metric, PrimitiveMetric::Score);

        const auto &recent = std::get<ProjectedFinalValue>(computedAt(configs, 8).expression->value());
        const auto &mean = std::get<RollingMean>(recent.input->value());
        EXPECT_EQ(mean.window, 5U);
        EXPECT_EQ(std::get<PrimitiveReference>(mean.input->value()).metric, PrimitiveMetric::Score);
    }

    TEST(SeriesConfigTest, ValidationReportsStableCodesAndFieldPaths) {
        auto configs = defaultSeriesConfigs();
        auto &accuracy = std::get<ComputedSeriesConfig>(configs[1]);
        accuracy.id = ComputedSeriesId{0};
        accuracy.presentation.name = " ";
        accuracy.presentation.lineStyle.width = std::numeric_limits<double>::infinity();
        accuracy.expression = {};

        const auto errors = validateSeriesConfigs(configs);
        EXPECT_TRUE(std::ranges::any_of(errors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::InvalidComputedSeriesId && error.path == "records[1].id";
        }));
        EXPECT_TRUE(std::ranges::any_of(errors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::EmptyComputedName && error.path == "records[1].presentation.name";
        }));
        EXPECT_TRUE(std::ranges::any_of(errors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::NonFiniteLineWidth &&
                   error.path == "records[1].presentation.lineStyle.width";
        }));
        EXPECT_TRUE(std::ranges::any_of(errors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::MissingExpressionInput && error.path == "records[1].expression";
        }));
    }

    TEST(SeriesConfigTest, ValidationRejectsInvalidExpressionParametersAndCollectionIntegrity) {
        auto configs = defaultSeriesConfigs();
        auto &accuracy = std::get<ComputedSeriesConfig>(configs[1]);
        accuracy.expression = rollingMean(primitive(PrimitiveMetric::Score), 0);
        std::get<BaseSeriesConfig>(configs[0]).presentation.name = "Renamed";
        std::get<BaseSeriesConfig>(configs[2]).metric = PrimitiveMetric::Score;
        std::get<ComputedSeriesConfig>(configs[6]).id = ComputedSeriesId{1};
        std::get<BaseSeriesConfig>(configs[3]).presentation.displayPosition = 8;

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
        const ComputedSeriesConfig config{ComputedSeriesId{99}, {"Copy", {}, true, 9}, copy};

        ASSERT_TRUE(validateSeriesConfig(config).empty());
        const auto &expression = std::get<Divide>(config.expression->value());
        EXPECT_EQ(std::get<PrimitiveReference>(expression.left->value()).metric, PrimitiveMetric::Hits);
        EXPECT_EQ(std::get<PrimitiveReference>(expression.right->value()).metric, PrimitiveMetric::Shots);
    }

    TEST(SeriesConfigTest, IndividualValidationCoversNamesStylesAndExpressionParameters) {
        const SeriesPresentation presentation{" name ", {{}, 0.25}, true, 0};
        const ComputedSeriesConfig config{
            ComputedSeriesId{9}, presentation,
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

        const ComputedSeriesConfig recent{ComputedSeriesId{10}, {std::string(121, 'x'), {}, true, 1},
                                           averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{0})};
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
        const auto deepErrors = validateSeriesConfig(ComputedSeriesConfig{ComputedSeriesId{9}, {"Deep", {}, true, 0}, deep});
        EXPECT_TRUE(std::ranges::any_of(deepErrors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::ExpressionDepthLimit;
        }));

        const std::function<Expression(size_t)> completeTree = [&](const size_t levels) -> Expression {
            if (levels == 0) return primitive(PrimitiveMetric::Score);
            return add(completeTree(levels - 1), completeTree(levels - 1));
        };
        const Expression large = completeTree(8);
        const auto largeErrors = validateSeriesConfig(ComputedSeriesConfig{ComputedSeriesId{10}, {"Large", {}, true, 0}, large});
        EXPECT_TRUE(std::ranges::any_of(largeErrors, [](const ValidationError &error) {
            return error.code == SeriesConfigValidationCode::ExpressionNodeLimit;
        }));
    }
}
