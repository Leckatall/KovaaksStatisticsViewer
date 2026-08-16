//
// Domain layer unit tests: ScenarioPerf, ScenarioId/ScenarioRunId, UserProfile.
//

#include <gtest/gtest.h>

#include <limits>
#include <unordered_set>

#include "scenario_perf.h"
#include "user_profile.h"

using namespace ksv::domain;

namespace {
    TEST(ScenarioPerfTest, AddDataInsertsNewPointForUnseenTime) {
        ScenarioPerf perf;
        perf.add_data(1.0F, SHOTS, 5);
        perf.add_data(2.0F, HITS, 3);

        ASSERT_EQ(perf.data.size(), 2);
        EXPECT_FLOAT_EQ(perf.data[0].time, 1.0F);
        EXPECT_EQ(perf.data[0].shots, 5);
        EXPECT_FLOAT_EQ(perf.data[1].time, 2.0F);
        EXPECT_EQ(perf.data[1].hits, 3);
    }

    TEST(ScenarioPerfTest, AddDataUpdatesExistingPointForSameTime) {
        ScenarioPerf perf;
        perf.add_data(1.0F, SHOTS, 5);
        perf.add_data(1.0F, HITS, 4);

        ASSERT_EQ(perf.data.size(), 1);
        EXPECT_EQ(perf.data[0].shots, 5);
        EXPECT_EQ(perf.data[0].hits, 4);
    }

    TEST(ScenarioPerfTest, AddDataSetsAllIntegralFields) {
        ScenarioPerf perf;
        perf.add_data(0.0F, SHOTS, 10);
        perf.add_data(0.0F, HITS, 8);
        perf.add_data(0.0F, MISSES, 2);
        perf.add_data(0.0F, KILLS, 1);

        ASSERT_EQ(perf.data.size(), 1);
        const auto &point = perf.data[0];
        EXPECT_EQ(point.shots, 10);
        EXPECT_EQ(point.hits, 8);
        EXPECT_EQ(point.misses, 2);
        EXPECT_EQ(point.kills, 1);
    }

    TEST(ScenarioPerfTest, AddDataSetsAllFloatingFields) {
        ScenarioPerf perf;
        perf.add_data(0.0F, DMG, 12.5F);
        perf.add_data(0.0F, DMG_POSSIBLE, 20.0F);
        perf.add_data(0.0F, SCORE, 99.9F);

        ASSERT_EQ(perf.data.size(), 1);
        const auto &point = perf.data[0];
        EXPECT_FLOAT_EQ(point.dmg, 12.5F);
        EXPECT_FLOAT_EQ(point.dmg_possible, 20.0F);
        EXPECT_FLOAT_EQ(point.score, 99.9F);
    }

    TEST(ScenarioPerfTest, AddDataThrowsWhenIntegralTypeGivenFloat) {
        ScenarioPerf perf;
        EXPECT_THROW(perf.add_data(0.0F, SHOTS, 5.0F), std::invalid_argument);
    }

    TEST(ScenarioPerfTest, AddDataThrowsWhenFloatingTypeGivenInt) {
        ScenarioPerf perf;
        EXPECT_THROW(perf.add_data(0.0F, SCORE, 5), std::invalid_argument);
    }

    TEST(ScenarioPerfTest, GetCompletionDataOnEmptyPerfIsAllZero) {
        ScenarioPerf perf;
        perf.scenario_length = 60.0F;

        const auto completion = perf.getRunData();

        EXPECT_EQ(completion.shots, 0);
        EXPECT_EQ(completion.hits, 0);
        EXPECT_EQ(completion.misses, 0);
        EXPECT_FLOAT_EQ(completion.dmg, 0.0F);
        EXPECT_FLOAT_EQ(completion.dmg_possible, 0.0F);
        EXPECT_FLOAT_EQ(completion.score, 0.0F);
        EXPECT_EQ(completion.kills, 0);
    }

    TEST(ScenarioPerfTest, GetCompletionDataSumsEachStatAcrossAllDataPoints) {
        // Real .perf files report per-tick values that are NOT cumulative running
        // totals (they can go up and down between ticks), so the true total for
        // a stat is the sum across every tick, not the last tick's value.
        ScenarioPerf perf;
        perf.scenario_length = 60.0F;
        perf.add_data(0.0F, SHOTS, 1);
        perf.add_data(0.0F, HITS, 1);
        perf.add_data(0.0F, MISSES, 0);
        perf.add_data(0.0F, DMG, 1.0F);
        perf.add_data(0.0F, DMG_POSSIBLE, 1.0F);
        perf.add_data(0.0F, SCORE, 1.0F);
        perf.add_data(0.0F, KILLS, 1);

        perf.add_data(1.0F, SHOTS, 4);
        perf.add_data(1.0F, HITS, 3);
        perf.add_data(1.0F, MISSES, 1);
        perf.add_data(1.0F, DMG, 4.0F);
        perf.add_data(1.0F, DMG_POSSIBLE, 4.0F);
        perf.add_data(1.0F, SCORE, 4.0F);
        perf.add_data(1.0F, KILLS, 4);

        const auto completion = perf.getRunData();

        EXPECT_EQ(completion.shots, 5);
        EXPECT_EQ(completion.hits, 4);
        EXPECT_EQ(completion.misses, 1);
        EXPECT_FLOAT_EQ(completion.dmg, 5.0F);
        EXPECT_FLOAT_EQ(completion.dmg_possible, 5.0F);
        EXPECT_FLOAT_EQ(completion.score, 5.0F);
        EXPECT_EQ(completion.kills, 5);
    }

    TEST(RunDataTest, AccuracyReturnsRatioAndHandlesZeroShots) {
        EXPECT_DOUBLE_EQ((RunData{.shots = 8, .hits = 6}.accuracy()), 0.75);
        EXPECT_DOUBLE_EQ((RunData{.shots = 0, .hits = 0}.accuracy()), 0.0);
    }

    TEST(ScenarioIdTest, EqualityIsHashOnlyIgnoringName) {
        // This is a deliberate (if surprising) behavior of ScenarioId::operator==:
        // two ids with the same hash are considered equal even if their names differ.
        const ScenarioId a{.name = "Scenario A", .hash = "abc123"};
        const ScenarioId b{.name = "Scenario B", .hash = "abc123"};

        EXPECT_TRUE(a == b);
    }

    TEST(ScenarioIdTest, InequalityWhenHashesDiffer) {
        const ScenarioId a{.name = "Scenario A", .hash = "abc123"};
        const ScenarioId b{.name = "Scenario A", .hash = "xyz789"};

        EXPECT_FALSE(a == b);
    }

    TEST(ScenarioIdTest, OrderingComparesByHash) {
        const ScenarioId a{.name = "A", .hash = "aaa"};
        const ScenarioId b{.name = "B", .hash = "bbb"};

        EXPECT_TRUE(a < b);
        EXPECT_FALSE(b < a);
    }

    TEST(ScenarioIdTest, HashIsConsistentWithEquality) {
        // Two ids that compare equal (same hash, different name) must land in the
        // same unordered_set bucket / be treated as duplicates.
        const ScenarioId a{.name = "Scenario A", .hash = "abc123"};
        const ScenarioId b{.name = "Scenario B", .hash = "abc123"};

        std::unordered_set<ScenarioId, std::hash<ScenarioId>> ids;
        ids.insert(a);
        ids.insert(b);

        // operator== for ScenarioId compiles fine for unordered_set membership tests,
        // but unordered_set also requires operator== (already defined) - inserting
        // "b" should not grow the set since it is equal to "a".
        EXPECT_EQ(ids.size(), 1);
    }

    TEST(ScenarioRunIdTest, EqualityRequiresSameScenarioAndStartTime) {
        const ScenarioRunId a{.scenario_id = {.name = "A", .hash = "h1"}, .start_time = 100};
        const ScenarioRunId b{.scenario_id = {.name = "A", .hash = "h1"}, .start_time = 100};
        const ScenarioRunId c{.scenario_id = {.name = "A", .hash = "h1"}, .start_time = 200};

        EXPECT_TRUE(a == b);
        EXPECT_FALSE(a == c);
    }

    TEST(ScenarioRunIdTest, ToStringFormatsNameDateAndTimeInLocalTime) {
        // start_time is epoch ms; toString renders it in the machine's local
        // timezone, so build the expected string the same way rather than
        // hard-coding a UTC value (which would fail on any off-UTC machine).
        constexpr long long start_ms = 1783733140000LL;
        const ScenarioRunId run_id{.scenario_id = {.name = "Air Angelic", .hash = "h1"}, .start_time = start_ms};

        const auto seconds = static_cast<std::time_t>(start_ms / 1000);
        std::tm local_tm{};
#ifdef _WIN32
        localtime_s(&local_tm, &seconds);
#else
        localtime_r(&seconds, &local_tm);
#endif
        std::ostringstream expected;
        expected << "Air Angelic (" << std::put_time(&local_tm, "%Y-%m-%d, %H:%M:%S") << ")";

        EXPECT_EQ(run_id.toString(), expected.str());
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
        static ScenarioPerf make_perf(const std::string &hash, long long start_time, float score = 0.0F) {
            ScenarioPerf perf;
            perf.run_id.scenario_id.name = "Scenario " + hash;
            perf.run_id.scenario_id.hash = hash;
            perf.run_id.start_time = start_time;
            perf.add_data(0.0F, SCORE, score);
            return perf;
        }
    };

    TEST_F(UserProfileTest, GroupsMultipleRunsUnderSameScenario) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-1", 200));

        const auto scenarios = profile.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "scenario-1");
    }

    TEST_F(UserProfileTest, ListsDistinctScenariosSeparately) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-2", 100));

        const auto scenarios = profile.getScenarioList();
        EXPECT_EQ(scenarios.size(), 2);
    }

    TEST_F(UserProfileTest, EmptyProfileHasNoScenarios) {
        const UserProfile profile;
        EXPECT_TRUE(profile.getScenarioList().empty());
    }

    TEST_F(UserProfileTest, GetMostRecentPerfReturnsNulloptWhenEmpty) {
        const UserProfile profile;
        EXPECT_FALSE(profile.getMostRecentPerf().has_value());
    }

    TEST_F(UserProfileTest, GetMostRecentPerfPicksLatestAcrossScenarios) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-2", 300));
        profile.addScenarioPerf(make_perf("scenario-1", 200));

        const auto latest = profile.getMostRecentPerf();
        ASSERT_TRUE(latest.has_value());
        EXPECT_EQ(latest->run_id.scenario_id.hash, "scenario-2");
        EXPECT_EQ(latest->run_id.start_time, 300);
    }

    TEST_F(UserProfileTest, GetMostRecentPerfForScenarioIgnoresInsertionOrder) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 200));
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-1", 300));

        const auto latest = profile.getMostRecentPerf(ScenarioId{.name = "Scenario scenario-1", .hash = "scenario-1"});
        ASSERT_TRUE(latest.has_value());
        EXPECT_EQ(latest->run_id.start_time, 300);
    }

    TEST_F(UserProfileTest, GetMostRecentPerfForUnknownScenarioIsNullopt) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100));

        EXPECT_FALSE(profile.getMostRecentPerf(ScenarioId{.name = "?", .hash = "unknown"}).has_value());
    }

    TEST_F(UserProfileTest, GetMostRecentPerfsReturnsUpToCountMostRecentInChronologicalOrder) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 300));
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-1", 200));
        profile.addScenarioPerf(make_perf("scenario-1", 400));

        const auto recent = profile.getMostRecentPerfs(ScenarioId{.name = "?", .hash = "scenario-1"}, 2);
        ASSERT_EQ(recent.size(), 2);
        EXPECT_EQ(recent[0].run_id.start_time, 300);
        EXPECT_EQ(recent[1].run_id.start_time, 400);
    }

    TEST_F(UserProfileTest, GetMostRecentPerfsClampsToAvailableRuns) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100));

        const auto recent = profile.getMostRecentPerfs(ScenarioId{.name = "?", .hash = "scenario-1"}, 5);
        EXPECT_EQ(recent.size(), 1);
    }

    TEST_F(UserProfileTest, GetCompletionHistoryProjectsOnlyMatchingRunsInChronologicalOrder) {
        UserProfile profile;
        auto latest = make_perf("scenario-1", 300, 30.0F);
        latest.add_data(0.0F, SHOTS, 12);
        latest.add_data(0.0F, HITS, 9);
        latest.add_data(0.0F, MISSES, 3);
        auto earliest = make_perf("scenario-1", 100, 10.0F);
        earliest.add_data(0.0F, SHOTS, 5);
        earliest.add_data(0.0F, HITS, 4);
        earliest.add_data(0.0F, MISSES, 1);
        profile.addScenarioPerf(latest);
        profile.addScenarioPerf(make_perf("scenario-2", 200, 99.0F));
        profile.addScenarioPerf(earliest);

        const auto history = profile.getCompletionHistory(
            ScenarioId{.name = "Different display name", .hash = "scenario-1"});

        ASSERT_EQ(history.size(), 2);
        EXPECT_EQ(history[0].run_id.start_time, 100);
        EXPECT_FLOAT_EQ(history[0].score, earliest.getRunData().score);
        EXPECT_EQ(history[0].shots, earliest.getRunData().shots);
        EXPECT_EQ(history[0].hits, earliest.getRunData().hits);
        EXPECT_EQ(history[0].misses, earliest.getRunData().misses);
        EXPECT_EQ(history[1].run_id.start_time, 300);
        EXPECT_FLOAT_EQ(history[1].score, latest.getRunData().score);
        EXPECT_EQ(history[1].shots, latest.getRunData().shots);
        EXPECT_EQ(history[1].hits, latest.getRunData().hits);
        EXPECT_EQ(history[1].misses, latest.getRunData().misses);
    }

    TEST_F(UserProfileTest, GetCompletionHistoryIsEmptyForUnknownScenario) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100));

        EXPECT_TRUE(profile.getCompletionHistory(ScenarioId{.name = "?", .hash = "unknown"}).empty());
    }

    TEST_F(UserProfileTest, GetAverageScoreAveragesFinalScoresOfMostRecentRuns) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100, 10.0F));
        profile.addScenarioPerf(make_perf("scenario-1", 200, 20.0F));
        profile.addScenarioPerf(make_perf("scenario-1", 300, 30.0F));

        const auto avg = profile.getAverageScore(ScenarioId{.name = "?", .hash = "scenario-1"}, 2);
        ASSERT_TRUE(avg.has_value());
        EXPECT_FLOAT_EQ(*avg, 25.0F); // average of the 2 most recent: 20 and 30
    }

    TEST_F(UserProfileTest, GetAverageScoreIsNulloptForUnknownScenario) {
        const UserProfile profile;
        EXPECT_FALSE(profile.getAverageScore(ScenarioId{.name = "?", .hash = "unknown"}, 3).has_value());
    }

    TEST(SourceRegistryTest, EnsureReturnsStableIdsAndResolvesChild) {
        SourceRegistry sources;

        const auto root = sources.ensure({}, "C:/Kovaaks");
        const auto repeated_root = sources.ensure({}, "C:/Kovaaks");
        const auto performances = sources.ensure(root, "FPSAimTrainer/performances");

        EXPECT_EQ(root, repeated_root);
        EXPECT_EQ(sources.resolve(performances), "C:/Kovaaks/FPSAimTrainer/performances");
        EXPECT_EQ(sources.resolve(SourceFileRef{performances, "run.perf"}),
                  "C:/Kovaaks/FPSAimTrainer/performances/run.perf");
    }

    TEST(SourceRegistryTest, RestoredIdsRemainStableForNewEntries) {
        SourceRegistry sources{{
            {{7}, {}, "C:/Kovaaks"},
            {{12}, {7}, "FPSAimTrainer/performances"}
        }};

        EXPECT_EQ(sources.ensure({}, "C:/Kovaaks"), DirectoryId{7});
        EXPECT_EQ(sources.ensure({}, "D:/Kovaaks"), DirectoryId{13});
    }

    TEST(SourceRegistryTest, ResolveRejectsCycles) {
        const SourceRegistry sources{{
            {{1}, {2}, "one"},
            {{2}, {1}, "two"}
        }};

        EXPECT_FALSE(sources.resolve(DirectoryId{1}).has_value());
    }

    TEST_F(UserProfileTest, EnsureSourceRegistersRootAndPerformancesDirectory) {
        UserProfile profile;

        const auto directory = profile.ensureSource("C:/Kovaaks", "FPSAimTrainer/performances");

        EXPECT_EQ(profile.sources().resolve(directory), "C:/Kovaaks/FPSAimTrainer/performances");
    }

    TEST_F(UserProfileTest, GetRunLooksUpByRunId) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100));

        const auto run = profile.getRun(ScenarioRunId{.scenario_id = {.name = "?", .hash = "scenario-1"}, .start_time = 100});
        ASSERT_TRUE(run.has_value());
        EXPECT_EQ(run->run_id.start_time, 100);
    }

    TEST_F(UserProfileTest, GetRunForUnknownRunIdIsNullopt) {
        const UserProfile profile;
        EXPECT_FALSE(profile.getRun(ScenarioRunId{.scenario_id = {.name = "?", .hash = "unknown"}, .start_time = 1}).has_value());
    }

    TEST_F(UserProfileTest, DuplicateRunIdIsSkippedAndReturnsFalse) {
        UserProfile profile;
        EXPECT_TRUE(profile.addScenarioPerf(make_perf("scenario-1", 100)));
        EXPECT_FALSE(profile.addScenarioPerf(make_perf("scenario-1", 100)));

        EXPECT_EQ(profile.getRunCount(ScenarioId{.name = "?", .hash = "scenario-1"}), 1);
    }

    TEST_F(UserProfileTest, GetTotalTimeSumsScenarioLengthsForScenario) {
        UserProfile profile;
        auto run_a = make_perf("scenario-1", 100);
        run_a.scenario_length = 30.0F;
        auto run_b = make_perf("scenario-1", 200);
        run_b.scenario_length = 45.0F;
        profile.addScenarioPerf(run_a);
        profile.addScenarioPerf(run_b);

        const auto total = profile.getTotalTime(ScenarioId{.name = "?", .hash = "scenario-1"});
        ASSERT_TRUE(total.has_value());
        EXPECT_DOUBLE_EQ(*total, 75.0);
    }

    TEST_F(UserProfileTest, GetTotalTimeIsNulloptForUnknownScenario) {
        const UserProfile profile;
        EXPECT_FALSE(profile.getTotalTime(ScenarioId{.name = "?", .hash = "unknown"}).has_value());
    }

    TEST_F(UserProfileTest, GetTotalTimeAllScenariosSumsAcrossScenarios) {
        UserProfile profile;
        auto run_a = make_perf("scenario-1", 100);
        run_a.scenario_length = 30.0F;
        auto run_b = make_perf("scenario-2", 100);
        run_b.scenario_length = 45.0F;
        profile.addScenarioPerf(run_a);
        profile.addScenarioPerf(run_b);

        EXPECT_DOUBLE_EQ(profile.getTotalTimeAllScenarios(), 75.0);
    }

    TEST_F(UserProfileTest, GetRunCountReturnsNumberOfRunsForScenario) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-1", 200));

        EXPECT_EQ(profile.getRunCount(ScenarioId{.name = "?", .hash = "scenario-1"}), 2);
    }

    TEST_F(UserProfileTest, GetLastRunTimeReturnsStartSecondOfMostRecentRun) {
        UserProfile profile;
        profile.addScenarioPerf(make_perf("scenario-1", 300000));
        profile.addScenarioPerf(make_perf("scenario-1", 100000));
        profile.addScenarioPerf(make_perf("scenario-1", 200000));

        const auto last_played = profile.getLastRunTime(ScenarioId{.name = "?", .hash = "scenario-1"});
        ASSERT_TRUE(last_played.has_value());
        EXPECT_EQ(*last_played, (ScenarioRunId{.scenario_id = {.name = "?", .hash = "scenario-1"}, .start_time = 300000}.startSecond()));
    }

    TEST_F(UserProfileTest, GetLastRunTimeIsNulloptForUnknownScenario) {
        const UserProfile profile;
        EXPECT_FALSE(profile.getLastRunTime(ScenarioId{.name = "?", .hash = "unknown"}).has_value());
    }

    TEST_F(UserProfileTest, GetRollingTimeAverageBucketsByCalendarDayWithGapDays) {
        UserProfile profile;
        constexpr long long kMsPerDay = 86400000;
        // A realistic epoch day (real runs always have positive timestamps; 0 is
        // reserved for the "unset" case the aggregation now rejects).
        constexpr long long kBaseDay = 19000;

        auto perf_day0 = make_perf("scenario-1", kBaseDay * kMsPerDay);
        perf_day0.scenario_length = 10.0F;
        auto perf_day2 = make_perf("scenario-1", (kBaseDay + 2) * kMsPerDay);
        perf_day2.scenario_length = 20.0F;
        profile.addScenarioPerf(perf_day0);
        profile.addScenarioPerf(perf_day2);

        const auto series = profile.getRollingTimeAverage(ScenarioId{.name = "?", .hash = "scenario-1"}, 2);

        // day1 has no run and must be treated as 0, not skipped, so the trailing
        // 2-day window is accurate on every day (including the partial first window).
        ASSERT_EQ(series.size(), 3);
        EXPECT_EQ(series[0].first.time_since_epoch().count(), kBaseDay);
        EXPECT_DOUBLE_EQ(series[0].second, 10.0);
        EXPECT_EQ(series[1].first.time_since_epoch().count(), kBaseDay + 1);
        EXPECT_DOUBLE_EQ(series[1].second, 5.0);
        EXPECT_EQ(series[2].first.time_since_epoch().count(), kBaseDay + 2);
        EXPECT_DOUBLE_EQ(series[2].second, 10.0);
    }

    TEST_F(UserProfileTest, GetRollingTimeAverageSkipsRunsWithNonPositiveStartTime) {
        // A default-constructed / malformed run carries start_time == 0 (the Unix
        // epoch, 1970). Including it would anchor the dense day-fill at 1970 and
        // stretch the series across decades of empty days. It must be ignored.
        UserProfile profile;
        constexpr long long kMsPerDay = 86400000;
        constexpr long long kBaseDay = 19000;

        auto bogus = make_perf("scenario-1", 0);
        bogus.scenario_length = 99.0F;
        auto real_run = make_perf("scenario-1", kBaseDay * kMsPerDay);
        real_run.scenario_length = 10.0F;
        profile.addScenarioPerf(bogus);
        profile.addScenarioPerf(real_run);

        const auto series = profile.getRollingTimeAverage(ScenarioId{.name = "?", .hash = "scenario-1"}, 3);

        // Only the real day survives; the 1970 run does not appear and does not
        // drag the series' first day back to the epoch.
        ASSERT_EQ(series.size(), 1);
        EXPECT_EQ(series[0].first.time_since_epoch().count(), kBaseDay);
        EXPECT_DOUBLE_EQ(series[0].second, 10.0);
    }

    TEST_F(UserProfileTest, GetRollingTimeAverageIsEmptyForUnknownScenario) {
        const UserProfile profile;
        EXPECT_TRUE(profile.getRollingTimeAverage(ScenarioId{.name = "?", .hash = "unknown"}, 7).empty());
    }

    TEST_F(UserProfileTest, GetRollingTimeAverageAcrossAllScenariosCombinesEveryScenario) {
        UserProfile profile;
        constexpr long long kMsPerDay = 86400000;

        // Two different scenarios both played on day 0; a third scenario alone on
        // day 2. The all-scenarios overload must sum across scenarios per day,
        // same gap-day and trailing-window semantics as the per-scenario one.
        // A realistic epoch day; both day-0 runs land on kBaseDay, the third two
        // days later (real runs always have positive timestamps).
        constexpr long long kBaseDay = 19000;
        auto run_a = make_perf("scenario-1", kBaseDay * kMsPerDay);
        run_a.scenario_length = 10.0F;
        auto run_b = make_perf("scenario-2", kBaseDay * kMsPerDay + 1000);
        run_b.scenario_length = 15.0F;
        auto run_c = make_perf("scenario-3", (kBaseDay + 2) * kMsPerDay);
        run_c.scenario_length = 20.0F;
        profile.addScenarioPerf(run_a);
        profile.addScenarioPerf(run_b);
        profile.addScenarioPerf(run_c);

        const auto series = profile.getRollingTimeAverage(2);

        ASSERT_EQ(series.size(), 3);
        EXPECT_EQ(series[0].first.time_since_epoch().count(), kBaseDay);
        EXPECT_DOUBLE_EQ(series[0].second, 25.0); // day0: 10 + 15
        EXPECT_EQ(series[1].first.time_since_epoch().count(), kBaseDay + 1);
        EXPECT_DOUBLE_EQ(series[1].second, 12.5); // (25 + 0) / 2
        EXPECT_EQ(series[2].first.time_since_epoch().count(), kBaseDay + 2);
        EXPECT_DOUBLE_EQ(series[2].second, 10.0); // (0 + 20) / 2
    }

    TEST_F(UserProfileTest, GetRollingTimeAverageAcrossAllScenariosIsEmptyWhenProfileEmpty) {
        const UserProfile profile;
        EXPECT_TRUE(profile.getRollingTimeAverage(7).empty());
    }

    TEST_F(UserProfileTest, GetRollingTimeAverageStaysSparseAcrossAWideTimestampGap) {
        UserProfile profile;
        constexpr long long kMsPerDay = 86400000;
        constexpr long long kBaseDay = 19000;
        constexpr long long kFarDay = kBaseDay + 1'000'000;

        auto near_run = make_perf("scenario-1", kBaseDay * kMsPerDay);
        near_run.scenario_length = 10.0F;
        auto far_run = make_perf("scenario-1", kFarDay * kMsPerDay);
        far_run.scenario_length = 20.0F;
        profile.addScenarioPerf(near_run);
        profile.addScenarioPerf(far_run);

        const auto series = profile.getRollingTimeAverage(ScenarioId{.name = "?", .hash = "scenario-1"}, 2);

        // Bounded by daily_totals.size() * window_days (2 runs * window 2), not the
        // ~1,000,000-day calendar span between them.
        EXPECT_LE(series.size(), 4);
    }
}
