//
// Domain layer unit tests: Run, ScenarioId/ScenarioRunId, UserProfile.
//

#include <gtest/gtest.h>

#include <limits>

#include "run.h"
#include "user_profile.h"

using namespace ksv::domain;

namespace {
    TEST(PerformanceTest, AddDataInsertsNewSampleForUnseenTime) {
        Performance performance;
        performance.add_data(1.0F, SHOTS, 5);
        performance.add_data(2.0F, HITS, 3);

        ASSERT_EQ(performance.samples.size(), 2);
        EXPECT_FLOAT_EQ(performance.samples[0].time, 1.0F);
        EXPECT_EQ(performance.samples[0].shots, 5);
        EXPECT_FLOAT_EQ(performance.samples[1].time, 2.0F);
        EXPECT_EQ(performance.samples[1].hits, 3);
    }

    TEST(PerformanceTest, AddDataUpdatesExistingSampleForSameTime) {
        Performance performance;
        performance.add_data(1.0F, SHOTS, 5);
        performance.add_data(1.0F, HITS, 4);

        ASSERT_EQ(performance.samples.size(), 1);
        EXPECT_EQ(performance.samples[0].shots, 5);
        EXPECT_EQ(performance.samples[0].hits, 4);
    }

    TEST(PerformanceTest, AddDataRejectsWrongValueTypeWithoutInsertingASample) {
        Performance performance;

        EXPECT_THROW(performance.add_data(0.0F, SHOTS, 5.0F), std::invalid_argument);
        EXPECT_TRUE(performance.samples.empty());
    }

    TEST(RunTotalsTest, AccuracyReturnsRatioAndHandlesZeroShots) {
        EXPECT_DOUBLE_EQ((ksv::domain::RunTotals{.shots = 8, .hits = 6}.accuracy()), 0.75);
        EXPECT_DOUBLE_EQ((ksv::domain::RunTotals{.shots = 0, .hits = 0}.accuracy()), 0.0);
    }

    TEST(RunTest, ExposesStoredTotalsWithoutRecomputingSamples) {
        ksv::domain::Run run;
        run.stored_totals = {.score = 73.0F, .shots = 10, .hits = 7, .misses = 3, .kills = 4};
        run.performance.emplace();
        run.performance->add_data(1.0F, SCORE, 10.0F);

        EXPECT_EQ(run.totals().score, 73.0F);
        EXPECT_EQ(run.totals().shots, 10);
    }

    TEST(ScenarioIdTest, EqualityIsHashOnlyIgnoringName) {
        EXPECT_EQ((ScenarioId{.name = "A", .hash = "hash"}),
                  (ScenarioId{.name = "B", .hash = "hash"}));
    }

    TEST(ScenarioRunIdTest, ToStringFallsBackToNameWhenLocalTimeConversionFails) {
        const ScenarioRunId run_id{
            .scenario_id = {.name = "Air Angelic", .hash = "h1"},
            .start_time = std::numeric_limits<long long>::max(),
        };

        EXPECT_EQ(run_id.toString(), "Air Angelic");
    }

    class UserProfileTest : public testing::Test {
    protected:
        static ksv::domain::Run makeRun(const std::string &hash, const long long start_time, const float score = 0.0F) {
            ksv::domain::Run run;
            run.run_id.scenario_id.name = "Scenario " + hash;
            run.run_id.scenario_id.hash = hash;
            run.run_id.start_time = start_time;
            run.stored_totals.score = score;
            return run;
        }
    };

    TEST_F(UserProfileTest, GroupsMultipleRunsUnderSameScenario) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 100));
        profile.addRun(makeRun("scenario-1", 200));

        const auto scenarios = profile.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "scenario-1");
    }

    TEST_F(UserProfileTest, GetLatestRunPicksLatestAcrossScenarios) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 100));
        profile.addRun(makeRun("scenario-2", 300));
        profile.addRun(makeRun("scenario-1", 200));

        const auto latest = profile.getLatestRun();
        ASSERT_TRUE(latest.has_value());
        EXPECT_EQ(latest->run_id.scenario_id.hash, "scenario-2");
        EXPECT_EQ(latest->run_id.start_time, 300);
    }

    TEST_F(UserProfileTest, GetMostRecentRunsReturnsUpToCountInChronologicalOrder) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 300));
        profile.addRun(makeRun("scenario-1", 100));
        profile.addRun(makeRun("scenario-1", 200));
        profile.addRun(makeRun("scenario-1", 400));

        const auto recent = profile.getMostRecentRuns(ScenarioId{.name = "?", .hash = "scenario-1"}, 2);
        ASSERT_EQ(recent.size(), 2);
        EXPECT_EQ(recent[0].run_id.start_time, 300);
        EXPECT_EQ(recent[1].run_id.start_time, 400);
    }

    TEST_F(UserProfileTest, CompletionHistoryUsesStoredTotals) {
        UserProfile profile;
        auto run = makeRun("scenario-1", 100, 97.0F);
        run.stored_totals = {.score = 97.0F, .shots = 12, .hits = 9, .misses = 3, .kills = 4};
        run.performance.emplace();
        run.performance->add_data(1.0F, SCORE, 2.0F);
        run.performance->add_data(1.0F, SHOTS, 1);
        profile.addRun(run);

        const auto history = profile.getCompletionHistory(
            ScenarioId{.name = "Different display name", .hash = "scenario-1"});

        ASSERT_EQ(history.size(), 1);
        EXPECT_EQ(history[0].run_id, run.run_id);
        EXPECT_EQ(history[0].totals, run.totals());
    }

    TEST_F(UserProfileTest, AverageScoreUsesStoredTotals) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 100, 10.0F));
        profile.addRun(makeRun("scenario-1", 200, 20.0F));
        profile.addRun(makeRun("scenario-1", 300, 30.0F));

        const auto average = profile.getAverageScore(ScenarioId{.name = "?", .hash = "scenario-1"}, 2);
        ASSERT_TRUE(average.has_value());
        EXPECT_FLOAT_EQ(*average, 25.0F);
    }

    TEST_F(UserProfileTest, GetCurrentRunLooksUpByRunId) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 100));

        const auto run = profile.getCurrentRun(ScenarioRunId{
            .scenario_id = {.name = "?", .hash = "scenario-1"}, .start_time = 100
        });
        ASSERT_TRUE(run.has_value());
        EXPECT_EQ(run->run_id.start_time, 100);
    }

    TEST_F(UserProfileTest, DuplicateRunIdIsSkippedAndReturnsFalse) {
        UserProfile profile;
        EXPECT_TRUE(profile.addRun(makeRun("scenario-1", 100)));
        EXPECT_FALSE(profile.addRun(makeRun("scenario-1", 100)));
        EXPECT_EQ(profile.getRunCount(ScenarioId{.name = "?", .hash = "scenario-1"}), 1);
    }

    TEST_F(UserProfileTest, TotalTimeAndRunCountRemainAggregatedPerScenario) {
        UserProfile profile;
        auto first = makeRun("scenario-1", 100);
        first.scenario_length = 30.0F;
        auto second = makeRun("scenario-1", 200);
        second.scenario_length = 45.0F;
        profile.addRun(first);
        profile.addRun(second);

        EXPECT_EQ(profile.getTotalTime(ScenarioId{.name = "?", .hash = "scenario-1"}), 75.0);
        EXPECT_EQ(profile.getRunCount(ScenarioId{.name = "?", .hash = "scenario-1"}), 2);
    }

    TEST_F(UserProfileTest, RollingTimeAverageSkipsRunsWithNonPositiveStartTime) {
        UserProfile profile;
        constexpr long long kMsPerDay = 86400000;
        constexpr long long kBaseDay = 19000;
        auto bogus = makeRun("scenario-1", 0);
        bogus.scenario_length = 99.0F;
        auto valid = makeRun("scenario-1", kBaseDay * kMsPerDay);
        valid.scenario_length = 10.0F;
        profile.addRun(bogus);
        profile.addRun(valid);

        const auto series = profile.getRollingTimeAverage(ScenarioId{.name = "?", .hash = "scenario-1"}, 3);
        ASSERT_EQ(series.size(), 1);
        EXPECT_EQ(series[0].first.time_since_epoch().count(), kBaseDay);
        EXPECT_DOUBLE_EQ(series[0].second, 10.0);
    }
}
