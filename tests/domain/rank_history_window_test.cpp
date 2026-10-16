#include <gtest/gtest.h>

#include <chrono>
#include <vector>

#include "benchmark_builders.h"
#include "benchmarks/benchmark_evaluator.h"
#include "benchmarks/rank_history.h"

using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    std::chrono::sys_days day(const int value) { return std::chrono::sys_days{std::chrono::days{value}}; }

    std::vector<AverageRankPoint> history() {
        return {{day(10), 1.0}, {day(12), 1.5}, {day(20), 2.0}, {day(25), 2.5}};
    }

    void expectPoint(const AverageRankPoint &point, const int expectedDay, const double expectedRank) {
        EXPECT_EQ(point.day, day(expectedDay));
        EXPECT_DOUBLE_EQ(point.averageRank, expectedRank);
    }
}

TEST(RankHistoryWindow, TheLastEarlierImprovementIsTheEstablishedRankAndInRangeOnesKeepTheirValues) {
    const auto full = history();
    const auto window = rankHistoryWindow(full, day(15), day(22));

    ASSERT_TRUE(window.established);
    expectPoint(*window.established, 12, 1.5);
    ASSERT_EQ(window.improvements.size(), 1U);
    expectPoint(window.improvements[0], 20, 2.0);
}

TEST(RankHistoryWindow, AWindowWithoutImprovementsStillHasItsEstablishedRank) {
    const auto full = history();
    const auto window = rankHistoryWindow(full, day(21), day(24));

    ASSERT_TRUE(window.established);
    expectPoint(*window.established, 20, 2.0);
    EXPECT_TRUE(window.improvements.empty());
}

TEST(RankHistoryWindow, NothingIsEstablishedBeforeTheFirstImprovement) {
    const auto full = history();

    const auto straddling = rankHistoryWindow(full, day(5), day(11));
    EXPECT_FALSE(straddling.established);
    ASSERT_EQ(straddling.improvements.size(), 1U);
    expectPoint(straddling.improvements[0], 10, 1.0);

    const auto before = rankHistoryWindow(full, day(1), day(9));
    EXPECT_FALSE(before.established);
    EXPECT_TRUE(before.improvements.empty());
}

TEST(RankHistoryWindow, BothEndsAreInclusive) {
    const auto full = history();
    const auto window = rankHistoryWindow(full, day(12), day(20));

    ASSERT_TRUE(window.established);
    expectPoint(*window.established, 10, 1.0);
    ASSERT_EQ(window.improvements.size(), 2U);
    expectPoint(window.improvements[0], 12, 1.5);
    expectPoint(window.improvements[1], 20, 2.0);
}

TEST(RankHistoryWindow, AWindowAfterTheLastImprovementCarriesTheCurrentRank) {
    const auto full = history();
    const auto window = rankHistoryWindow(full, day(30), day(40));

    ASSERT_TRUE(window.established);
    expectPoint(*window.established, 25, 2.5);
    EXPECT_TRUE(window.improvements.empty());
}

TEST(RankHistoryWindow, WindowedValuesAreTheFullEvaluationsValues) {
    Benchmark definition{BenchmarkId{"bench"}, "Bench",
                         {{TierId{"bronze"}, "bronze", {}}, {TierId{"silver"}, "silver", {}}, {TierId{"gold"}, "gold", {}}},
                         {}, {}};
    const std::initializer_list<std::pair<std::string, double> > rungs{
        {"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}};
    definition.uncategorized = {benchmarkEntry("one", "One", "one", rungs), benchmarkEntry("two", "Two", "two", rungs)};
    constexpr long long kDay = 86'400'000LL;
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 150.0F), benchmarkFact("two", 101 * kDay, 120.0F),
                                   benchmarkFact("one", 105 * kDay, 260.0F), benchmarkFact("two", 108 * kDay, 300.0F)};
    const auto projection = evaluateBenchmark(definition, {Completeness::Trackable, {}},
                                              {resolvedEntry("one", "one"), resolvedEntry("two", "two")}, facts, {});
    ASSERT_EQ(projection.averageRankHistory.size(), 4U);

    // A rank already reached before the window keeps the value the whole history gave it.
    const auto window = rankHistoryWindow(projection.averageRankHistory, day(104), day(106));
    ASSERT_TRUE(window.established);
    EXPECT_DOUBLE_EQ(window.established->averageRank, projection.averageRankHistory[1].averageRank);
    ASSERT_EQ(window.improvements.size(), 1U);
    EXPECT_DOUBLE_EQ(window.improvements[0].averageRank, projection.averageRankHistory[2].averageRank);
}
