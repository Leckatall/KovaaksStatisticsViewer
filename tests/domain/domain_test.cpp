//
// Domain layer unit tests: Run, ScenarioId/ScenarioRunId, UserProfile.
//

#include <gtest/gtest.h>

#include <limits>

#include "run.h"
#include "user_profile.h"

#include "run_builders.h"

using namespace ksv::domain;
using ksv::tests_support::makeRun;

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

    TEST(UserProfileTest, GroupsMultipleRunsUnderSameScenario) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 100));
        profile.addRun(makeRun("scenario-1", 200));

        const auto scenarios = profile.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "scenario-1");
    }

    TEST(UserProfileTest, GetLatestRunPicksLatestAcrossScenarios) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 100));
        profile.addRun(makeRun("scenario-2", 300));
        profile.addRun(makeRun("scenario-1", 200));

        const auto latest = profile.getLatestRun();
        ASSERT_TRUE(latest.has_value());
        EXPECT_EQ(latest->run_id.scenario_id.hash, "scenario-2");
        EXPECT_EQ(latest->run_id.start_time, 300);
    }

    TEST(UserProfileTest, GetMostRecentRunsReturnsUpToCountInChronologicalOrder) {
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

    TEST(UserProfileTest, CompletionHistoryUsesStoredTotals) {
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

    TEST(UserProfileTest, AverageScoreUsesStoredTotals) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 100, 10.0F));
        profile.addRun(makeRun("scenario-1", 200, 20.0F));
        profile.addRun(makeRun("scenario-1", 300, 30.0F));

        const auto average = profile.getAverageScore(ScenarioId{.name = "?", .hash = "scenario-1"}, 2);
        ASSERT_TRUE(average.has_value());
        EXPECT_FLOAT_EQ(*average, 25.0F);
    }

    TEST(UserProfileTest, GetCurrentRunLooksUpByRunId) {
        UserProfile profile;
        profile.addRun(makeRun("scenario-1", 100));

        const auto run = profile.getCurrentRun(ScenarioRunId{
            .scenario_id = {.name = "?", .hash = "scenario-1"}, .start_time = 100
        });
        ASSERT_TRUE(run.has_value());
        EXPECT_EQ(run->run_id.start_time, 100);
    }

    TEST(UserProfileImportTest, LateCsvEnrichesOneRunWithoutLosingSamples) {
        UserProfile profile;
        auto original = makeRun("h", 100, 10, 5, 3, 30);
        original.performance.emplace();
        original.performance->add_data(1.0F, HITS, 3);
        original.sources.perf = {profile.ensureSource("C:/Kovaaks", "FPSAimTrainer/performances"), "run.perf"};
        ASSERT_TRUE(profile.addRun(original));
        auto incoming = makeRun("h", 100, 20, 8, 6, 35);
        incoming.stats = Stats{};
        incoming.sources.csv = {profile.ensureSource("C:/Kovaaks", "FPSAimTrainer/stats"), "run Stats.csv"};
        EXPECT_EQ(profile.mergeImportedRun(incoming), ImportMerge::Enriched);
        EXPECT_EQ(profile.getRunCount(original.run_id.scenario_id), 1U);
        EXPECT_DOUBLE_EQ(profile.getTotalTimeAllScenarios(), 30);
        const auto merged = profile.getCurrentRun(original.run_id);
        ASSERT_TRUE(merged);
        EXPECT_FLOAT_EQ(merged->stored_totals.score, 20);
        ASSERT_TRUE(merged->performance);
        EXPECT_EQ(merged->performance->samples[0].hits, 3);
        EXPECT_EQ(profile.mergeImportedRun(incoming), ImportMerge::Unchanged);
    }

    TEST(UserProfileImportTest, LatePerfRekeysCsvRunAndUpdatesOrderingAndDuration) {
        UserProfile profile;
        auto csv = makeRun("h", 100, 20, 8, 6, 30);
        csv.sources.csv = {profile.ensureSource("C:/Kovaaks", "FPSAimTrainer/stats"), "run Stats.csv"};
        csv.stats = Stats{};
        profile.addRun(csv);
        profile.addRun(makeRun("h", 150, 5, 1, 1, 10));
        auto perf = makeRun("h", 200, 10, 5, 3, 35);
        perf.sources.perf = {profile.ensureSource("C:/Kovaaks", "FPSAimTrainer/performances"), "run.perf"};
        perf.performance.emplace();
        EXPECT_EQ(profile.mergeImportedRun(perf, csv.run_id), ImportMerge::Enriched);
        EXPECT_FALSE(profile.getCurrentRun(csv.run_id));
        ASSERT_TRUE(profile.getLatestRun());
        EXPECT_EQ(profile.getLatestRun()->run_id.start_time, 200);
        EXPECT_EQ(profile.getMostRecentRun(csv.run_id.scenario_id)->run_id.start_time, 200);
        EXPECT_FLOAT_EQ(profile.getCurrentRun(perf.run_id)->stored_totals.score, 20);
        EXPECT_EQ(profile.getRunCount(csv.run_id.scenario_id), 2U);
        EXPECT_DOUBLE_EQ(profile.getTotalTimeAllScenarios(), 45);
    }

    TEST(UserProfileImportTest, RekeyCollisionLeavesBothRunsUnchanged) {
        UserProfile profile;
        const auto old = makeRun("h", 100, 10, 0, 0, 30);
        const auto other = makeRun("h", 200, 20, 0, 0, 40);
        profile.addRun(old);
        profile.addRun(other);
        EXPECT_EQ(profile.mergeImportedRun(other, old.run_id), ImportMerge::Conflict);
        EXPECT_EQ(profile.getRunCount(old.run_id.scenario_id), 2U);
        EXPECT_DOUBLE_EQ(profile.getTotalTimeAllScenarios(), 70);
        EXPECT_FLOAT_EQ(profile.getCurrentRun(old.run_id)->stored_totals.score, 10);
    }

    TEST(UserProfileTest, FailedSourcesRoundTripAndSurviveACopy) {
        UserProfile profile;
        const auto directory = profile.ensureSource("C:/Kovaaks", "FPSAimTrainer/performances");
        const std::vector<FailedSource> failed{
            {{directory, "b.perf"}, 2, 20, "second"},
            {{directory, "a.perf"}, 1, 10, "first"},
        };
        profile.setFailedSources(failed);
        EXPECT_EQ(profile.failedSources(), failed);
        const UserProfile copy = profile;
        EXPECT_EQ(copy.failedSources(), failed);
    }

    TEST(UserProfileTest, DuplicateRunIdIsSkippedAndReturnsFalse) {
        UserProfile profile;
        EXPECT_TRUE(profile.addRun(makeRun("scenario-1", 100)));
        EXPECT_FALSE(profile.addRun(makeRun("scenario-1", 100)));
        EXPECT_EQ(profile.getRunCount(ScenarioId{.name = "?", .hash = "scenario-1"}), 1);
    }

    TEST(UserProfileTest, TotalTimeAndRunCountRemainAggregatedPerScenario) {
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

    TEST(UserProfileTest, RollingTimeAverageSkipsRunsWithNonPositiveStartTime) {
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
