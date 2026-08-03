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
        static ScenarioPerf make_perf(const std::string &hash, long long start_time) {
            ScenarioPerf perf;
            perf.run_id.scenario_id.name = "Scenario " + hash;
            perf.run_id.scenario_id.hash = hash;
            perf.run_id.start_time = start_time;
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
}
