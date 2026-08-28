#include <gtest/gtest.h>

#include <vector>

#include "app/contracts/expression_dsl.h"
#include "app/contracts/series_config.h"
#include "series_expression_qml.h"

using namespace ksv::application;
using ksv::presentation::expressionMap;
using ksv::presentation::parseExpression;

namespace {
    // Every expression the DSL codec must represent identically to the existing JSON codec. Anchored on
    // the real shipped trees (defaultSeriesConfigs) plus edge cases the defaults don't cover: both
    // selection variants, a negative/fractional constant, and depth beyond anything in the catalogue.
    std::vector<Expression> corpus() {
        std::vector<Expression> expressions;
        for (const auto &config: defaultSeriesConfigs()) expressions.push_back(config.expression);
        expressions.push_back(numericConstant(-3.25));
        expressions.push_back(add(primitive(PrimitiveMetric::Score), numericConstant(0.5)));
        expressions.push_back(averageAcrossRuns(primitive(PrimitiveMetric::Score), RecentRuns{20}));
        expressions.push_back(averageAcrossRuns(primitive(PrimitiveMetric::Hits), TopPercentile{12.5}));
        expressions.push_back(divide(
            averageAcrossRuns(runningSum(primitive(PrimitiveMetric::Score)), RecentRuns{20}),
            rollingMean(subtract(primitive(PrimitiveMetric::Hits), numericConstant(1.0)), 3)));
        return expressions;
    }

    // The DSL and JSON codecs must canonicalize the same tree to the same DSL string: routing a tree
    // through JSON and back must not change what the DSL codec makes of it.
    TEST(ExpressionDslDifferential, JsonAndDslCodecsAgreeAcrossCorpus) {
        for (const auto &expression: corpus()) {
            const auto canonical = encodeExpressionDsl(expression);

            const auto viaJson = parseExpression(expressionMap(expression));
            ASSERT_TRUE(viaJson.has_value()) << "JSON codec rejected: " << canonical;
            EXPECT_EQ(encodeExpressionDsl(*viaJson), canonical);

            const auto viaDsl = decodeExpressionDsl(canonical);
            ASSERT_TRUE(viaDsl.has_value()) << "DSL codec rejected its own output: " << canonical;
            EXPECT_EQ(encodeExpressionDsl(*viaDsl), canonical);
        }
    }
}
