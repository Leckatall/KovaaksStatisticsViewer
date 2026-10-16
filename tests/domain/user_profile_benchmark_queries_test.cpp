#include <gtest/gtest.h>

#include "run_builders.h"
#include "user_profile.h"

using namespace ksv;
using namespace ksv::domain;
using ksv::tests_support::makeRun;

namespace {
    constexpr long long kDay = 86'400'000LL;

    ScenarioId scenario(const std::string &hash) { return {"Scenario " + hash, hash}; }
}

TEST(UserProfileBenchmarkQueries, RunFactsMergeRequestedScenariosIntoOneChronologicalSequence) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 10.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("b", 101 * kDay, 20.0F, 0, 0, 30.0F));
    profile.addRun(makeRun("a", 102 * kDay, 30.0F, 0, 0, 45.0F));
    profile.addRun(makeRun("c", 103 * kDay, 40.0F, 0, 0, 90.0F));

    const auto facts = profile.getRunFacts({scenario("a"), scenario("b")});

    ASSERT_EQ(facts.size(), 3U);
    EXPECT_EQ(facts[0].run_id.scenario_id.hash, "a");
    EXPECT_EQ(facts[0].run_id.start_time, 100 * kDay);
    EXPECT_EQ(facts[1].run_id.scenario_id.hash, "b");
    EXPECT_EQ(facts[2].run_id.scenario_id.hash, "a");
    EXPECT_EQ(facts[2].run_id.start_time, 102 * kDay);
    EXPECT_FLOAT_EQ(facts[1].score, 20.0F);
    EXPECT_FLOAT_EQ(facts[1].duration_seconds, 30.0F);
}

TEST(UserProfileBenchmarkQueries, RunFactsCountAScenarioOnceWhenRequestedUnderTwoNames) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 10.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("a", 101 * kDay, 20.0F, 0, 0, 60.0F));

    const auto facts = profile.getRunFacts({{"One", "a"}, {"Another name", "a"}});

    EXPECT_EQ(facts.size(), 2U);
}

TEST(UserProfileBenchmarkQueries, RunFactsBreakStartTimeTiesByAppendOrder) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 10.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("b", 100 * kDay, 20.0F, 0, 0, 30.0F));

    const auto facts = profile.getRunFacts({scenario("a"), scenario("b")});

    ASSERT_EQ(facts.size(), 2U);
    EXPECT_EQ(facts[0].run_id.scenario_id.hash, "a");
    EXPECT_EQ(facts[1].run_id.scenario_id.hash, "b");
}

TEST(UserProfileBenchmarkQueries, RunFactsIgnoreScenariosWithNoRuns) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 10.0F, 0, 0, 60.0F));

    const auto facts = profile.getRunFacts({scenario("a"), scenario("never-played")});

    ASSERT_EQ(facts.size(), 1U);
    EXPECT_EQ(facts[0].run_id.scenario_id.hash, "a");
}

TEST(UserProfileBenchmarkQueries, RunFactsForNoScenariosAreEmpty) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 10.0F, 0, 0, 60.0F));

    EXPECT_TRUE(profile.getRunFacts({}).empty());
}

TEST(UserProfileBenchmarkQueries, RollingAverageOverEveryScenarioEqualsTheProfileWideSeries) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 10.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("b", 100 * kDay, 20.0F, 0, 0, 120.0F));
    profile.addRun(makeRun("a", 101 * kDay, 30.0F, 0, 0, 30.0F));
    profile.addRun(makeRun("b", 104 * kDay, 40.0F, 0, 0, 90.0F));

    EXPECT_EQ(profile.getRollingTimeAverage(profile.getScenarioList(), 3),
              profile.getRollingTimeAverage(3));
}

TEST(UserProfileBenchmarkQueries, RollingAverageExcludesScenariosOutsideTheRequestedSet) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 10.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("b", 100 * kDay, 20.0F, 0, 0, 600.0F));

    const auto filtered = profile.getRollingTimeAverage(std::vector<ScenarioId>{scenario("a")}, 3);

    ASSERT_EQ(filtered.size(), 1U);
    EXPECT_DOUBLE_EQ(filtered[0].second, 60.0);
}

TEST(UserProfileBenchmarkQueries, RollingAverageForNoScenariosIsEmpty) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 10.0F, 0, 0, 60.0F));

    EXPECT_TRUE(profile.getRollingTimeAverage(std::vector<ScenarioId>{}, 3).empty());
}

namespace {
    using Series = std::vector<std::pair<std::chrono::sys_days, double> >;

    std::chrono::sys_days epochDay(const long long day) { return std::chrono::sys_days{std::chrono::days{day}}; }
}

TEST(UserProfileRollingWindow, OneDayWindowIsTheDailyTotalsOfDaysWithRuns) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 0.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("b", 100 * kDay + 5'000, 0.0F, 0, 0, 120.0F));
    profile.addRun(makeRun("a", 101 * kDay, 0.0F, 0, 0, 30.0F));
    profile.addRun(makeRun("b", 104 * kDay, 0.0F, 0, 0, 90.0F));

    EXPECT_EQ(profile.getRollingTimeAverage(1),
              (Series{{epochDay(100), 180.0}, {epochDay(101), 30.0}, {epochDay(104), 90.0}}));
}

TEST(UserProfileRollingWindow, WindowLongerThanTheHistoryKeepsTheShorterInitialDenominator) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 0.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("a", 102 * kDay, 0.0F, 0, 0, 60.0F));

    EXPECT_EQ(profile.getRollingTimeAverage(3650),
              (Series{{epochDay(100), 60.0}, {epochDay(101), 30.0}, {epochDay(102), 40.0}}));
}

TEST(UserProfileRollingWindow, DaysWhoseWindowHoldsNoRunsAreOmittedForAnyWindow) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 0.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("a", 105 * kDay, 0.0F, 0, 0, 60.0F));

    EXPECT_EQ(profile.getRollingTimeAverage(2),
              (Series{{epochDay(100), 60.0}, {epochDay(101), 30.0}, {epochDay(105), 30.0}}));
}

TEST(UserProfileRollingWindow, OutputStorageIsBoundedByTheHistoryNotTheWindow) {
    UserProfile profile;
    profile.addRun(makeRun("a", 100 * kDay, 0.0F, 0, 0, 60.0F));
    profile.addRun(makeRun("a", 103 * kDay, 0.0F, 0, 0, 60.0F));

    const auto series = profile.getRollingTimeAverage(3650);

    EXPECT_LE(series.capacity(), 4U);
}
