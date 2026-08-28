#include <gtest/gtest.h>

#include "app/contracts/expression_dsl.h"

using namespace ksv::application;

namespace {
    TEST(ExpressionDslEncode, PrimitiveMetricsAreFullUpper) {
        EXPECT_EQ(encodeExpressionDsl(primitive(PrimitiveMetric::Score)), "SCORE");
        EXPECT_EQ(encodeExpressionDsl(primitive(PrimitiveMetric::Shots)), "SHOTS");
        EXPECT_EQ(encodeExpressionDsl(primitive(PrimitiveMetric::Hits)), "HITS");
        EXPECT_EQ(encodeExpressionDsl(primitive(PrimitiveMetric::Kills)), "KILLS");
        EXPECT_EQ(encodeExpressionDsl(primitive(PrimitiveMetric::Dmg)), "DMG");
    }

    TEST(ExpressionDslEncode, ConstantsUseShortestRoundTripForm) {
        EXPECT_EQ(encodeExpressionDsl(numericConstant(2.0)), "2");
        EXPECT_EQ(encodeExpressionDsl(numericConstant(0.5)), "0.5");
        EXPECT_EQ(encodeExpressionDsl(numericConstant(-3.25)), "-3.25");
    }

    TEST(ExpressionDslEncode, BinaryOperatorsArePascalCasePrefix) {
        const auto s = primitive(PrimitiveMetric::Score);
        const auto h = primitive(PrimitiveMetric::Hits);
        EXPECT_EQ(encodeExpressionDsl(add(s, h)), "Add(SCORE, HITS)");
        EXPECT_EQ(encodeExpressionDsl(subtract(s, h)), "Subtract(SCORE, HITS)");
        EXPECT_EQ(encodeExpressionDsl(multiply(s, h)), "Multiply(SCORE, HITS)");
        EXPECT_EQ(encodeExpressionDsl(divide(s, h)), "Divide(SCORE, HITS)");
    }

    TEST(ExpressionDslEncode, UnaryOperatorsArePascalCasePrefix) {
        const auto s = primitive(PrimitiveMetric::Score);
        EXPECT_EQ(encodeExpressionDsl(runningSum(s)), "RunningSum(SCORE)");
        EXPECT_EQ(encodeExpressionDsl(projectedFinalValue(s)), "ProjectedFinalValue(SCORE)");
        EXPECT_EQ(encodeExpressionDsl(projectRateToFinal(s)), "ProjectRateToFinal(SCORE)");
    }

    TEST(ExpressionDslEncode, RollingMeanLabelsWindowFirst) {
        EXPECT_EQ(encodeExpressionDsl(rollingMean(primitive(PrimitiveMetric::Score), 10)),
                  "RollingMean(window: 10, SCORE)");
    }

    TEST(ExpressionDslEncode, AverageAcrossRunsRecentSelection) {
        EXPECT_EQ(encodeExpressionDsl(averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{5})),
                  "AverageAcrossRuns(over: recent 5, SCORE)");
    }

    TEST(ExpressionDslEncode, AverageAcrossRunsTopPercentileSelection) {
        EXPECT_EQ(encodeExpressionDsl(averageAcrossRuns(primitive(PrimitiveMetric::Hits), TopPercentile{10.0})),
                  "AverageAcrossRuns(over: top 10%, HITS)");
    }

    TEST(ExpressionDslEncode, NestedExpressionEncodesRecursively) {
        const auto expr = divide(
            runningSum(primitive(PrimitiveMetric::Score)),
            rollingMean(primitive(PrimitiveMetric::Hits), 10));
        EXPECT_EQ(encodeExpressionDsl(expr), "Divide(RunningSum(SCORE), RollingMean(window: 10, HITS))");
    }

    // A canonical string re-encodes to itself after a decode; comparing encodings is the round-trip
    // oracle, since Expression has no deep structural equality.
    void expectRoundTrips(const Expression &expr) {
        const auto encoded = encodeExpressionDsl(expr);
        const auto decoded = decodeExpressionDsl(encoded);
        ASSERT_TRUE(decoded.has_value()) << "failed to decode: " << encoded;
        EXPECT_EQ(encodeExpressionDsl(*decoded), encoded);
    }

    TEST(ExpressionDslDecode, RoundTripsEveryNodeType) {
        expectRoundTrips(primitive(PrimitiveMetric::Kills));
        expectRoundTrips(numericConstant(-3.25));
        expectRoundTrips(add(primitive(PrimitiveMetric::Score), numericConstant(2.0)));
        expectRoundTrips(subtract(primitive(PrimitiveMetric::Score), primitive(PrimitiveMetric::Hits)));
        expectRoundTrips(multiply(primitive(PrimitiveMetric::Score), numericConstant(0.5)));
        expectRoundTrips(divide(primitive(PrimitiveMetric::Hits), primitive(PrimitiveMetric::Shots)));
        expectRoundTrips(runningSum(primitive(PrimitiveMetric::Score)));
        expectRoundTrips(projectedFinalValue(primitive(PrimitiveMetric::Score)));
        expectRoundTrips(projectRateToFinal(primitive(PrimitiveMetric::Score)));
        expectRoundTrips(rollingMean(primitive(PrimitiveMetric::Score), 10));
        expectRoundTrips(averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{5}));
        expectRoundTrips(averageAcrossRuns(primitive(PrimitiveMetric::Hits), TopPercentile{12.5}));
    }

    TEST(ExpressionDslDecode, RoundTripsDeeplyNestedExpression) {
        expectRoundTrips(divide(
            averageAcrossRuns(runningSum(primitive(PrimitiveMetric::Score)), RecentRuns{20}),
            rollingMean(subtract(primitive(PrimitiveMetric::Hits), numericConstant(1.0)), 3)));
    }

    TEST(ExpressionDslDecode, IsCaseInsensitiveForNamesKeywordsAndMetrics) {
        const auto decoded = decodeExpressionDsl("add(score, ROLLINGMEAN(WINDOW: 10, hits))");
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(encodeExpressionDsl(*decoded), "Add(SCORE, RollingMean(window: 10, HITS))");
    }

    TEST(ExpressionDslDecode, ToleratesArbitraryWhitespaceBetweenTokens) {
        const auto decoded = decodeExpressionDsl("  Divide(  RunningSum( SCORE ) ,RollingMean(window:10,HITS ) )  ");
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(encodeExpressionDsl(*decoded), "Divide(RunningSum(SCORE), RollingMean(window: 10, HITS))");
    }

    TEST(ExpressionDslDecode, DecodesTopPercentileSelection) {
        const auto decoded = decodeExpressionDsl("AverageAcrossRuns(over: top 10%, HITS)");
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(encodeExpressionDsl(*decoded), "AverageAcrossRuns(over: top 10%, HITS)");
    }

    TEST(ExpressionDslDecode, RejectsMalformedInput) {
        EXPECT_FALSE(decodeExpressionDsl("").has_value());
        EXPECT_FALSE(decodeExpressionDsl("Bogus(SCORE)").has_value());
        EXPECT_FALSE(decodeExpressionDsl("Add(SCORE)").has_value());              // too few args
        EXPECT_FALSE(decodeExpressionDsl("Add(SCORE, HITS, KILLS)").has_value()); // too many args
        EXPECT_FALSE(decodeExpressionDsl("RunningSum(SCORE").has_value());        // unbalanced paren
        EXPECT_FALSE(decodeExpressionDsl("Add(SCORE, HITS) trailing").has_value());
        EXPECT_FALSE(decodeExpressionDsl("RollingMean(10, SCORE)").has_value());  // missing window: label
        EXPECT_FALSE(decodeExpressionDsl("RollingMean(window: 0, SCORE)").has_value()); // window must be > 0
        EXPECT_FALSE(decodeExpressionDsl("AverageAcrossRuns(over: recent 0, SCORE)").has_value());
        EXPECT_FALSE(decodeExpressionDsl("PONTIAC").has_value());                 // unknown metric
    }

    TEST(ExpressionDslDecode, RejectsPositionalWindowWhenLabelRequired) {
        EXPECT_FALSE(decodeExpressionDsl("RollingMean(SCORE, window: 10)").has_value());
    }
}
