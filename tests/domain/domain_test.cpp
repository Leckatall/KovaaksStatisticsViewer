//
// Domain layer unit tests: ScenarioPerf, ScenarioId/ScenarioRunId, UserProfile.
//

#include <gtest/gtest.h>

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

    TEST(ScenarioPerfTest, GetCompletionDataOnEmptyPerfIsAllZeroButUsesScenarioLength) {
        ScenarioPerf perf;
        perf.scenario_length = 60.0F;

        const auto completion = perf.getCompletionData();

        EXPECT_FLOAT_EQ(completion.scenario_time, 60.0F);
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

        const auto completion = perf.getCompletionData();

        EXPECT_FLOAT_EQ(completion.scenario_time, 60.0F);
        EXPECT_EQ(completion.shots, 5);
        EXPECT_EQ(completion.hits, 4);
        EXPECT_EQ(completion.misses, 1);
        EXPECT_FLOAT_EQ(completion.dmg, 5.0F);
        EXPECT_FLOAT_EQ(completion.dmg_possible, 5.0F);
        EXPECT_FLOAT_EQ(completion.score, 5.0F);
        EXPECT_EQ(completion.kills, 5);
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
        UserProfile profile{"default"};
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-1", 200));

        const auto scenarios = profile.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "scenario-1");
    }

    TEST_F(UserProfileTest, ListsDistinctScenariosSeparately) {
        UserProfile profile{"default"};
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-2", 100));

        const auto scenarios = profile.getScenarioList();
        EXPECT_EQ(scenarios.size(), 2);
    }

    TEST_F(UserProfileTest, EmptyProfileHasNoScenarios) {
        const UserProfile profile{"default"};
        EXPECT_TRUE(profile.getScenarioList().empty());
    }

    TEST_F(UserProfileTest, GetMostRecentPerfReturnsNulloptWhenEmpty) {
        const UserProfile profile{"default"};
        EXPECT_FALSE(profile.getMostRecentPerf().has_value());
    }

    TEST_F(UserProfileTest, GetMostRecentPerfPicksLatestAcrossScenarios) {
        UserProfile profile{"default"};
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-2", 300));
        profile.addScenarioPerf(make_perf("scenario-1", 200));

        const auto latest = profile.getMostRecentPerf();
        ASSERT_TRUE(latest.has_value());
        EXPECT_EQ(latest->run_id.scenario_id.hash, "scenario-2");
        EXPECT_EQ(latest->run_id.start_time, 300);
    }

    TEST_F(UserProfileTest, GetMostRecentPerfForScenarioIgnoresInsertionOrder) {
        UserProfile profile{"default"};
        profile.addScenarioPerf(make_perf("scenario-1", 200));
        profile.addScenarioPerf(make_perf("scenario-1", 100));
        profile.addScenarioPerf(make_perf("scenario-1", 300));

        const auto latest = profile.getMostRecentPerf(ScenarioId{.name = "Scenario scenario-1", .hash = "scenario-1"});
        ASSERT_TRUE(latest.has_value());
        EXPECT_EQ(latest->run_id.start_time, 300);
    }

    TEST_F(UserProfileTest, GetMostRecentPerfForUnknownScenarioIsNullopt) {
        UserProfile profile{"default"};
        profile.addScenarioPerf(make_perf("scenario-1", 100));

        EXPECT_FALSE(profile.getMostRecentPerf(ScenarioId{.name = "?", .hash = "unknown"}).has_value());
    }

    TEST_F(UserProfileTest, GetMostRecentPerfsReturnsUpToCountMostRecentInChronologicalOrder) {
        UserProfile profile{"default"};
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
        UserProfile profile{"default"};
        profile.addScenarioPerf(make_perf("scenario-1", 100));

        const auto recent = profile.getMostRecentPerfs(ScenarioId{.name = "?", .hash = "scenario-1"}, 5);
        EXPECT_EQ(recent.size(), 1);
    }

    TEST_F(UserProfileTest, GetAverageScoreAveragesFinalScoresOfMostRecentRuns) {
        UserProfile profile{"default"};
        profile.addScenarioPerf(make_perf("scenario-1", 100, 10.0F));
        profile.addScenarioPerf(make_perf("scenario-1", 200, 20.0F));
        profile.addScenarioPerf(make_perf("scenario-1", 300, 30.0F));

        const auto avg = profile.getAverageScore(ScenarioId{.name = "?", .hash = "scenario-1"}, 2);
        ASSERT_TRUE(avg.has_value());
        EXPECT_FLOAT_EQ(*avg, 25.0F); // average of the 2 most recent: 20 and 30
    }

    TEST_F(UserProfileTest, GetAverageScoreIsNulloptForUnknownScenario) {
        const UserProfile profile{"default"};
        EXPECT_FALSE(profile.getAverageScore(ScenarioId{.name = "?", .hash = "unknown"}, 3).has_value());
    }
}
