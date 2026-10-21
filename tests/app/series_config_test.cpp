#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <string_view>
#include <variant>

#include "app/contracts/expression_dsl.h"
#include "app/contracts/graph_configuration.h"
#include "app/contracts/series_config.h"

using namespace ksv::application;

namespace {
    bool hasError(const std::vector<ValidationError> &errors, const SeriesConfigValidationCode code,
                  const std::string_view path = {}) {
        return std::ranges::any_of(errors, [code, path](const ValidationError &error) {
            return error.code == code && (path.empty() || error.path == path);
        });
    }

    // Dense display positions and unique ids, so a test can mutate one line without depending on the shipped lineup.
    std::vector<SeriesConfig> smallLineup() {
        std::vector<SeriesConfig> configs;
        for (uint32_t index = 0; index < 4; ++index) {
            configs.push_back({
                SeriesId{index + 1}, {"Line " + std::to_string(index + 1), {}, true, index},
                primitive(PrimitiveMetric::Score)
            });
        }
        return configs;
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

    TEST(SeriesConfigTest, DefaultRunSeriesDocumentIsValid) {
        const auto document = defaultRunSeriesDocument();

        EXPECT_TRUE(validateRunSeriesDocument(document).empty());
        EXPECT_TRUE(validateSeriesConfigs(document.series).empty());
        EXPECT_TRUE(sameSeriesConfigs(document.series, defaultSeriesConfigs()));
        EXPECT_TRUE(sameAxisConfigs(document.axes, defaultAxisConfigs()));

        std::set<uint64_t> axisIds;
        for (const auto &axis: document.axes) EXPECT_TRUE(axisIds.insert(axis.id.value).second) << axis.name;

        std::set<uint64_t> seriesIds;
        uint64_t highestSeriesId = 0;
        for (size_t index = 0; index < document.series.size(); ++index) {
            const auto &series = document.series[index];
            SCOPED_TRACE(series.presentation.name);
            EXPECT_TRUE(seriesIds.insert(series.id.value).second);
            EXPECT_EQ(series.presentation.displayPosition, index);
            if (series.yAxisId) EXPECT_TRUE(axisIds.contains(series.yAxisId->value));
            highestSeriesId = std::max(highestSeriesId, series.id.value);
        }

        ASSERT_TRUE(document.nextSeriesId);
        EXPECT_EQ(document.nextSeriesId->value, highestSeriesId + 1);
        ASSERT_TRUE(document.nextAxisId);
        EXPECT_EQ(document.nextAxisId->value, *axisIds.rbegin() + 1);
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
        auto configs = smallLineup();
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
        auto configs = smallLineup();
        configs[1].presentation.hiddenLeadingPoints = 0;
        EXPECT_TRUE(validateSeriesConfigs(configs).empty());
        configs[1].presentation.hiddenLeadingPoints = kMaximumHiddenLeadingPoints;
        EXPECT_TRUE(validateSeriesConfigs(configs).empty());

        configs[1].presentation.hiddenLeadingPoints = kMaximumHiddenLeadingPoints + 1;
        EXPECT_TRUE(hasError(validateSeriesConfigs(configs), SeriesConfigValidationCode::HiddenLeadingPointsOutOfRange,
                             "records[1].presentation.hiddenLeadingPoints"));
    }

    TEST(SeriesConfigTest, SameSeriesConfigsNoticesAHiddenLeadingPointsDifference) {
        const auto left = smallLineup();
        auto right = left;
        EXPECT_TRUE(sameSeriesConfigs(left, right));
        right[2].presentation.hiddenLeadingPoints = 3;
        EXPECT_FALSE(sameSeriesConfigs(left, right));
    }

    TEST(SeriesConfigTest, BlankTopLevelExpressionIsAllowedButNestedMissingOperandIsNot) {
        auto configs = smallLineup();
        configs[0].expression = {};
        EXPECT_TRUE(validateSeriesConfigs(configs).empty());

        configs[1].expression = add({}, primitive(PrimitiveMetric::Score));
        const auto errors = validateSeriesConfigs(configs);
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::MissingExpressionInput,
                             "records[1].expression.left"));
    }

    TEST(SeriesConfigTest, ValidationRejectsInvalidExpressionParametersAndCollectionIntegrity) {
        auto configs = smallLineup();
        configs[1].expression = rollingMean(primitive(PrimitiveMetric::Score), 0);
        configs[3].id = configs[0].id;
        configs[2].presentation.displayPosition = configs.back().presentation.displayPosition;

        const auto errors = validateSeriesConfigs(configs);
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::InvalidRollingWindow));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::DuplicateComputedSeriesId));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::DuplicateDisplayPosition));
        EXPECT_TRUE(hasError(errors, SeriesConfigValidationCode::NonDenseDisplayPosition));
    }

    TEST(SeriesConfigTest, DuplicatePrimitiveReferencesAreNoLongerRejected) {
        auto configs = smallLineup();
        configs[2].presentation.name = "Kills Again";
        configs[2].expression = primitive(PrimitiveMetric::Kills);
        configs[3].expression = primitive(PrimitiveMetric::Kills);

        EXPECT_TRUE(validateSeriesConfigs(configs).empty());
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
