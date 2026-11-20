#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <vector>

#include "rolling_playtime.h"
#include "run_builders.h"
#include "user_profile.h"

using namespace std::chrono;
using namespace ksv::domain;
using ksv::tests_support::makeRun;

namespace {
    using Series = std::vector<std::pair<sys_days, double>>;
    constexpr long long kDay = 86'400'000LL;

    sys_days epochDay(const int day) { return sys_days{days{day}}; }
}

TEST(RollingPlaytime, OneDayWindowReportsDailyTotalsOnActiveDays) {
    const std::array runs{TimedScore{100 * kDay, 0, 60}, TimedScore{100 * kDay + 5000, 0, 120},
                          TimedScore{101 * kDay, 0, 30}, TimedScore{104 * kDay, 0, 90}};

    EXPECT_EQ(rollingPlaytime(runs, 1),
              (Series{{epochDay(100), 180.0}, {epochDay(101), 30.0}, {epochDay(104), 90.0}}));
}

TEST(RollingPlaytime, LongWindowUsesTheShorterInitialDenominator) {
    const std::array runs{TimedScore{100 * kDay, 0, 60}, TimedScore{102 * kDay, 0, 60}};

    EXPECT_EQ(rollingPlaytime(runs, 3650),
              (Series{{epochDay(100), 60.0}, {epochDay(101), 30.0}, {epochDay(102), 40.0}}));
}

TEST(RollingPlaytime, OmitsDaysWithNoRunsInTheirWindow) {
    const std::array runs{TimedScore{100 * kDay, 0, 60}, TimedScore{105 * kDay, 0, 60}};

    EXPECT_EQ(rollingPlaytime(runs, 2),
              (Series{{epochDay(100), 60.0}, {epochDay(101), 30.0}, {epochDay(105), 30.0}}));
}

TEST(RollingPlaytime, EmptyInputOrNonPositiveWindowProducesNoSamples) {
    const std::array runs{TimedScore{100 * kDay, 0, 60}};

    EXPECT_TRUE(rollingPlaytime({}, 2).empty());
    EXPECT_TRUE(rollingPlaytime(runs, 0).empty());
    EXPECT_TRUE(rollingPlaytime(runs, -3).empty());
}

TEST(RollingPlaytime, IgnoresRunsAtAndBeforeTheEpoch) {
    const std::array runs{TimedScore{-1000, 0, 80}, TimedScore{0, 0, 40}, TimedScore{1000, 0, 15}};

    EXPECT_EQ(rollingPlaytime(runs, 3), (Series{{epochDay(0), 15.0}}));
    EXPECT_TRUE(rollingPlaytime(std::span<const TimedScore>{runs}.first(2), 3).empty());
}

TEST(RollingPlaytime, OutputStorageIsBoundedByTheHistory) {
    const std::array runs{TimedScore{100 * kDay, 0, 60}, TimedScore{103 * kDay, 0, 60}};
    const auto series = rollingPlaytime(runs, 3650);

    ASSERT_EQ(series.size(), 4U);
    EXPECT_LE(series.capacity(), 4U);
}

TEST(RollingPlaytime, AgreesWithTheProfileSeriesForMergedScenarioRuns) {
    UserProfile profile;
    profile.addRun(makeRun("a", 104 * kDay, 12.0F, 0, 0, 29.25F));
    profile.addRun(makeRun("b", 100 * kDay, 34.0F, 0, 0, 31.5F));
    profile.addRun(makeRun("a", 100 * kDay, 40.0F, 0, 0, 12.0F));
    profile.addRun(makeRun("c", 101 * kDay + 2000, 8.0F, 0, 0, 13.75F));
    profile.addRun(makeRun("b", -1000, 50.0F, 0, 0, 60.0F));

    std::vector<TimedScore> runs;
    for (const auto &fact: profile.getRunFacts(profile.getScenarioList())) runs.push_back(fact.timed());

    for (const int window: {1, 2, 3, 30}) {
        SCOPED_TRACE(window);
        EXPECT_EQ(rollingPlaytime(runs, window), profile.getRollingTimeAverage(window));
    }
}
