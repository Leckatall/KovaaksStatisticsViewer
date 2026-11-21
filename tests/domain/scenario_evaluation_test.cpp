#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

#include "run.h"
#include "scenario_evaluation.h"

using namespace ksv::domain;

TEST(TimedScore, CopiesTheNumericFieldsFromTheRunFact) {
    const RunFact fact{{{"Scenario", "hash"}, 123'456}, 27.5F, 63.25F};
    const auto timed = fact.timed();

    EXPECT_EQ(timed.start_time, fact.run_id.start_time);
    EXPECT_FLOAT_EQ(timed.score, fact.score);
    EXPECT_FLOAT_EQ(timed.duration_seconds, fact.duration_seconds);
}

TEST(TimedScore, FloorsTimestampsLikeScenarioRunId) {
    for (const long long stamp: {86'400'000LL, 86'400'321LL, 0LL, -1LL, -86'400'001LL}) {
        SCOPED_TRACE(stamp);
        const ScenarioRunId id{{}, stamp};
        const TimedScore timed{stamp};

        EXPECT_EQ(timed.startSecond(), id.startSecond());
        EXPECT_EQ(timed.startDay(), id.startDay());
    }
}

using namespace std::chrono;

TEST(ScenarioEvaluation, NoRunsHaveNoEvaluation) {
    EXPECT_EQ(ScenarioEvaluation::fromRuns({}), std::nullopt);
}

TEST(ScenarioEvaluation, BestScoreTieKeepsTheEarliestRun) {
    const std::array runs{TimedScore{2'125, 12}, TimedScore{3'750, 40}, TimedScore{4'500, 40}};
    const auto evaluation = ScenarioEvaluation::fromRuns(runs);

    ASSERT_TRUE(evaluation);
    EXPECT_EQ(evaluation->personalBest(), 40.0);
    EXPECT_EQ(evaluation->personalBestTime(), sys_seconds{seconds{3}});
}

TEST(ScenarioEvaluation, RecentWindowKeepsOnlyItsLatestRunsAndPreservesThePersonalBest) {
    constexpr auto window = ScenarioEvaluation::kRecentRuns;
    std::vector<TimedScore> runs{{0, 100}};
    for (int index = 1; index <= window; ++index) {
        runs.push_back({static_cast<long long>(index) * 1000, static_cast<float>(index)});
    }

    for (const int count: {window - 1, window, window + 1}) {
        SCOPED_TRACE(count);
        const int sampleCount = std::min(count, window);
        const double sum = 0.5 * (count - 1) * count + (count <= window ? 100.0 : 0.0);
        const auto evaluation =
            ScenarioEvaluation::fromRuns(std::span<const TimedScore>{runs}.first(static_cast<std::size_t>(count)));

        ASSERT_TRUE(evaluation);
        EXPECT_EQ(evaluation->recentSampleCount(), sampleCount);
        EXPECT_DOUBLE_EQ(evaluation->recentAverage(), sum / sampleCount);
        EXPECT_EQ(evaluation->personalBest(), 100.0);
    }
}

TEST(ScenarioEvaluation, InterleavedHashesContributeToOneEvaluation) {
    const RunFact first{{{"Old version", "old"}, 1'500}, 60, 12.5F};
    const RunFact second{{{"New version", "new"}, 2'500}, 10, 20};
    const RunFact third{{{"Old version", "old"}, 3'500}, 35, 3.25F};
    const RunFact fourth{{{"New version", "new"}, 4'500}, 60, 4.25F};
    const std::array runs{first.timed(), second.timed(), third.timed(), fourth.timed()};
    const auto evaluation = ScenarioEvaluation::fromRuns(runs);

    ASSERT_TRUE(evaluation);
    EXPECT_EQ(evaluation->runCount(), 4);
    EXPECT_EQ(evaluation->personalBest(), 60.0);
    EXPECT_EQ(evaluation->personalBestTime(), sys_seconds{seconds{1}});
    EXPECT_EQ(evaluation->recentSampleCount(), 4);
    EXPECT_EQ(evaluation->recentAverage(), 41.25);
    EXPECT_DOUBLE_EQ(evaluation->playtimeSeconds(), 40.0);
}

TEST(ScenarioEvaluation, PlaytimeIncludesRunsAtAndBeforeTheEpoch) {
    const std::array runs{TimedScore{-1000, 1, 6.5F}, TimedScore{0, 2, 7.25F}, TimedScore{1, 3, 1.25F}};
    const auto evaluation = ScenarioEvaluation::fromRuns(runs);

    ASSERT_TRUE(evaluation);
    EXPECT_DOUBLE_EQ(evaluation->playtimeSeconds(), 15.0);
}

TEST(ScenarioEvaluation, PersonalBestHistoryHoldsTheFirstRunAndEveryStrictImprovement) {
    const std::array runs{TimedScore{1'000, 100}, TimedScore{2'000, 90}, TimedScore{3'000, 150},
                          TimedScore{4'000, 150}, TimedScore{5'000, 200}};
    const auto evaluation = ScenarioEvaluation::fromRuns(runs);

    ASSERT_TRUE(evaluation);
    const std::vector<PersonalBestEntry> expected{{1, 100.0}, {3, 150.0}, {5, 200.0}};
    EXPECT_EQ(evaluation->personalBestHistory(), expected);
}

TEST(ScenarioEvaluation, RollingRecentAverageGrowsToTheWindowThenSlides) {
    constexpr auto window = ScenarioEvaluation::kRecentRuns;
    std::vector<TimedScore> runs;
    for (int index = 1; index <= window + 1; ++index)
        runs.push_back({static_cast<long long>(index) * 1000, static_cast<float>(index * 10)});
    const auto evaluation = ScenarioEvaluation::fromRuns(runs);

    ASSERT_TRUE(evaluation);
    const auto &averages = evaluation->recentAverages();
    ASSERT_EQ(averages.size(), runs.size());
    for (int count = 1; count <= window; ++count) {
        SCOPED_TRACE(count);
        EXPECT_DOUBLE_EQ(averages[static_cast<std::size_t>(count - 1)], 10.0 * (count + 1) / 2.0);
    }
    // Run window + 1 drops run 1: the mean of 20..(window + 1) * 10.
    EXPECT_DOUBLE_EQ(averages.back(), 10.0 * (window + 3) / 2.0);
}

TEST(ScenarioEvaluation, CurrentValuesAreTheLastEntriesOfTheSeries) {
    const std::array runs{TimedScore{1'000, 12.5F}, TimedScore{2'500, 40.25F}, TimedScore{3'000, 7},
                          TimedScore{4'750, 33.3F}, TimedScore{5'000, 40.25F}, TimedScore{6'000, 19.9F},
                          TimedScore{7'000, 1.1F}};
    const auto evaluation = ScenarioEvaluation::fromRuns(runs);

    ASSERT_TRUE(evaluation);
    ASSERT_FALSE(evaluation->personalBestHistory().empty());
    const auto &lastBest = evaluation->personalBestHistory().back();
    EXPECT_EQ(evaluation->personalBest(), lastBest.score);
    EXPECT_EQ(evaluation->personalBestTime(), runs[static_cast<std::size_t>(lastBest.scenarioRun - 1)].startSecond());
    ASSERT_FALSE(evaluation->recentAverages().empty());
    EXPECT_EQ(evaluation->recentAverage(), evaluation->recentAverages().back());
}

TEST(ScenarioEvaluation, ZeroScoreIsDistinctFromUnplayed) {
    const std::array zeroScore{TimedScore{1}};
    const std::array otherScore{TimedScore{1, 1}};
    const auto played = ScenarioEvaluation::fromRuns(zeroScore);

    ASSERT_TRUE(played);
    EXPECT_EQ(played->personalBest(), 0.0);
    EXPECT_EQ(played, ScenarioEvaluation::fromRuns(zeroScore));
    EXPECT_NE(played, ScenarioEvaluation::fromRuns(otherScore));
}
