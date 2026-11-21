#include <gtest/gtest.h>

#include <chrono>
#include <vector>

#include "benchmark_builders.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/rank_history.h"

using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    std::chrono::sys_days day(const int value) { return std::chrono::sys_days{std::chrono::days{value}}; }

    std::vector<AverageRankOfDay> history() {
        return {{day(10), 1.0}, {day(12), 1.5}, {day(20), 2.0}, {day(25), 2.5}};
    }

    void expectPoint(const AverageRankOfDay &point, const int expectedDay, const double expectedRank) {
        EXPECT_EQ(point.day, day(expectedDay));
        EXPECT_DOUBLE_EQ(point.averageRank, expectedRank);
    }

    std::chrono::sys_seconds at(const int dayValue, const int hour) {
        return std::chrono::sys_seconds{day(dayValue)} + std::chrono::hours{hour};
    }

    // Runs 1-6 over days 10, 10, 12, 12, 12 and 15.
    std::vector<AverageRankAfterRun> runHistory() {
        return {{at(10, 1), 1, 1.0}, {at(10, 23), 2, 1.2}, {at(12, 0), 3, 1.8},
                {at(12, 5), 4, 2.4}, {at(12, 9), 5, 1.6}, {at(15, 12), 6, 2.0}};
    }

    void expectRun(const AverageRankAfterRun &point, const int expectedRun, const double expectedRank) {
        EXPECT_EQ(point.benchmarkRun, expectedRun);
        EXPECT_DOUBLE_EQ(point.averageRank, expectedRank);
    }
}

TEST(RankHistoryByDay, ByDayKeepsTheLastPointOfEachDay) {
    const auto full = runHistory();
    const auto byDay = rankHistoryByDay(full);

    // Day 12 rose to 2.4 and fell back: the day reads its last run, not its highest.
    ASSERT_EQ(byDay.size(), 3U);
    expectPoint(byDay[0], 10, 1.2);
    expectPoint(byDay[1], 12, 1.6);
    expectPoint(byDay[2], 15, 2.0);
}

TEST(RankHistoryByDay, ByDayOfNothingIsNothing) {
    EXPECT_TRUE(rankHistoryByDay({}).empty());
}

TEST(RankHistoryRunWindow, RunWindowCarriesTheLastEarlierPointAndKeepsInSpanPoints) {
    const auto full = runHistory();
    const auto window = rankHistoryWindow(full, RunSpan{3, 5});

    ASSERT_TRUE(window.established);
    expectRun(*window.established, 2, 1.2);
    ASSERT_EQ(window.points.size(), 3U);
    expectRun(window.points[0], 3, 1.8);
    expectRun(window.points[2], 5, 1.6);
}

TEST(RankHistoryRunWindow, RunWindowBeforeAnyPointHasNothingEstablished) {
    const std::vector<AverageRankAfterRun> sparse{{at(10, 0), 4, 1.0}, {at(11, 0), 7, 1.5}};
    const auto window = rankHistoryWindow(sparse, RunSpan{1, 5});

    EXPECT_FALSE(window.established);
    ASSERT_EQ(window.points.size(), 1U);
    expectRun(window.points[0], 4, 1.0);
}

TEST(RunSpanOfDays, RunSpanOfDaysCoversTheRunsOfThoseDays) {
    const std::vector starts{at(10, 0), at(10, 23), at(12, 0), at(12, 23), at(15, 0)};

    const auto middle = runSpanOfDays(starts, day(11), day(12));
    ASSERT_TRUE(middle);
    EXPECT_EQ(middle->first, 3);
    EXPECT_EQ(middle->last, 4);

    const auto inclusive = runSpanOfDays(starts, day(10), day(15));
    ASSERT_TRUE(inclusive);
    EXPECT_EQ(inclusive->first, 1);
    EXPECT_EQ(inclusive->last, 5);
}

TEST(RunSpanOfDays, RunSpanOfDaysWithoutRunsIsNothing) {
    const std::vector starts{at(10, 0), at(12, 0)};
    EXPECT_FALSE(runSpanOfDays(starts, day(11), day(11)));
    EXPECT_FALSE(runSpanOfDays(starts, day(13), day(20)));
    EXPECT_FALSE(runSpanOfDays({}, day(1), day(20)));
}

TEST(RankHistoryWindow, TheLastEarlierImprovementIsTheEstablishedRankAndInRangeOnesKeepTheirValues) {
    const auto full = history();
    const auto window = rankHistoryWindow(full, day(15), day(22));

    ASSERT_TRUE(window.established);
    expectPoint(*window.established, 12, 1.5);
    ASSERT_EQ(window.points.size(), 1U);
    expectPoint(window.points[0], 20, 2.0);
}

TEST(RankHistoryWindow, AWindowWithoutImprovementsStillHasItsEstablishedRank) {
    const auto full = history();
    const auto window = rankHistoryWindow(full, day(21), day(24));

    ASSERT_TRUE(window.established);
    expectPoint(*window.established, 20, 2.0);
    EXPECT_TRUE(window.points.empty());
}

TEST(RankHistoryWindow, NothingIsEstablishedBeforeTheFirstImprovement) {
    const auto full = history();

    const auto straddling = rankHistoryWindow(full, day(5), day(11));
    EXPECT_FALSE(straddling.established);
    ASSERT_EQ(straddling.points.size(), 1U);
    expectPoint(straddling.points[0], 10, 1.0);

    const auto before = rankHistoryWindow(full, day(1), day(9));
    EXPECT_FALSE(before.established);
    EXPECT_TRUE(before.points.empty());
}

TEST(RankHistoryWindow, BothEndsAreInclusive) {
    const auto full = history();
    const auto window = rankHistoryWindow(full, day(12), day(20));

    ASSERT_TRUE(window.established);
    expectPoint(*window.established, 10, 1.0);
    ASSERT_EQ(window.points.size(), 2U);
    expectPoint(window.points[0], 12, 1.5);
    expectPoint(window.points[1], 20, 2.0);
}

TEST(RankHistoryWindow, AWindowAfterTheLastImprovementCarriesTheCurrentRank) {
    const auto full = history();
    const auto window = rankHistoryWindow(full, day(30), day(40));

    ASSERT_TRUE(window.established);
    expectPoint(*window.established, 25, 2.5);
    EXPECT_TRUE(window.points.empty());
}

TEST(RankHistoryWindow, WindowedValuesAreTheFullEvaluationsValues) {
    BenchmarkDefinition definition{BenchmarkId{"bench"}, "Bench",
                         {{TierId{"bronze"}, "bronze", {}}, {TierId{"silver"}, "silver", {}}, {TierId{"gold"}, "gold", {}}},
                         {}, {}};
    const std::initializer_list<std::pair<std::string, double> > rungs{
        {"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}};
    definition.uncategorized = {benchmarkEntry("one", "One", "one", rungs), benchmarkEntry("two", "Two", "two", rungs)};
    constexpr long long kDay = 86'400'000LL;
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 150.0F), benchmarkFact("two", 101 * kDay, 120.0F),
                                   benchmarkFact("one", 105 * kDay, 260.0F), benchmarkFact("two", 108 * kDay, 300.0F)};
    const auto evaluated = Benchmark(definition, {Completeness::Trackable, {}},
                                     {resolvedEntry("one"), resolvedEntry("two")}, facts);
    const auto byDay = rankHistoryByDay(evaluated.averageRankHistory());
    ASSERT_EQ(byDay.size(), 4U);

    // A rank already reached before the window keeps the value the whole history gave it.
    const auto window = rankHistoryWindow(byDay, day(104), day(106));
    ASSERT_TRUE(window.established);
    EXPECT_DOUBLE_EQ(window.established->averageRank, byDay[1].averageRank);
    ASSERT_EQ(window.points.size(), 1U);
    EXPECT_DOUBLE_EQ(window.points[0].averageRank, byDay[2].averageRank);
}
