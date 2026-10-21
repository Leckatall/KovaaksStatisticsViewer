#include <gtest/gtest.h>

#include <array>
#include <span>
#include <vector>

#include "app/contracts/range_projection.h"
#include "run.h"
#include "fake_day_source.h"

using namespace ksv::application;
using namespace std::chrono;

namespace {
    constexpr sys_days day(const int y, const unsigned m, const unsigned d) {
        return sys_days{year{y} / month{m} / std::chrono::day{d}};
    }

    constexpr year_month_day date(const int y, const unsigned m, const unsigned d) {
        return year{y} / month{m} / std::chrono::day{d};
    }

    RangePreference preference(GraphRange range, const bool anchorToLatestRun = false) {
        return RangePreference{std::move(range), anchorToLatestRun};
    }

    RangeProjection interval(const sys_days first, const sys_days last) { return DayInterval{first, last}; }

    constexpr auto kToday = day(2026, 10, 16);

    TEST(ResolveRange, AllHistoryNeedsNoAnchorEvenWhenAnchoringIsOn) {
        EXPECT_EQ(resolveRange(preference(AllHistory{}), kToday, std::nullopt), RangeProjection{EntireHistory{}});
        EXPECT_EQ(resolveRange(preference(AllHistory{}, true), kToday, std::nullopt),
                  RangeProjection{EntireHistory{}});
    }

    TEST(ResolveRange, RecentDaysEndTodayAndCountTodayAsTheFirstOfNDays) {
        const auto projection = resolveRange(preference(RecentDays{7}), kToday, day(2026, 1, 1));
        EXPECT_EQ(projection, interval(day(2026, 10, 10), kToday));
    }

    TEST(ResolveRange, OneRecentDayIsJustTheAnchorDay) {
        const auto projection = resolveRange(preference(RecentDays{1}), kToday, std::nullopt);
        EXPECT_EQ(projection, interval(kToday, kToday));
    }

    TEST(ResolveRange, AnchorFlagMovesRecentDaysToTheLatestRelevantRun) {
        const auto latest = day(2026, 3, 5);
        const auto projection = resolveRange(preference(RecentDays{3}, true), kToday, latest);
        EXPECT_EQ(projection, interval(day(2026, 3, 3), latest));
    }

    TEST(ResolveRange, AnchorFlagOnWithoutAnyRelevantRunIsNoRelevantHistoryNotToday) {
        EXPECT_EQ(resolveRange(preference(RecentDays{3}, true), kToday, std::nullopt),
                  RangeProjection{NoRelevantHistory{}});
    }

    TEST(ResolveRange, RecentDaysCrossMonthAndYearBoundaries) {
        const auto projection = resolveRange(preference(RecentDays{5}), day(2027, 1, 2), std::nullopt);
        EXPECT_EQ(projection, interval(day(2026, 12, 29), day(2027, 1, 2)));
    }

    TEST(ResolveRange, CustomDatesAreInclusiveAndIgnoreTheAnchor) {
        const auto custom = CustomDates{date(2026, 2, 1), date(2026, 2, 28)};
        const RangeProjection expected{DayInterval{day(2026, 2, 1), day(2026, 2, 28)}};
        EXPECT_EQ(resolveRange(preference(custom), kToday, std::nullopt), expected);
        EXPECT_EQ(resolveRange(preference(custom, true), kToday, day(2025, 1, 1)), expected);
        EXPECT_EQ(resolveRange(preference(custom, true), kToday, std::nullopt), expected);
    }

    TEST(ResolveRange, SingleDayCustomRangeContainsExactlyThatDay) {
        const auto projection = std::get<DayInterval>(
            resolveRange(preference(CustomDates{date(2026, 4, 4), date(2026, 4, 4)}), kToday, std::nullopt));
        EXPECT_TRUE(projection.contains(day(2026, 4, 4)));
        EXPECT_FALSE(projection.contains(day(2026, 4, 3)));
        EXPECT_FALSE(projection.contains(day(2026, 4, 5)));
    }

    TEST(ResolveRange, LatestRunsIgnoreTheAnchorAndStayResolvableWithoutRuns) {
        EXPECT_EQ(resolveRange(preference(LatestRuns{5}, true), kToday, std::nullopt),
                  RangeProjection{LatestRunSuffix{5}});
        EXPECT_EQ(resolveRange(preference(LatestRuns{5}), kToday, day(2020, 1, 1)),
                  RangeProjection{LatestRunSuffix{5}});
    }

    TEST(DayInterval, EndpointsAreInclusive) {
        const DayInterval interval{day(2026, 5, 1), day(2026, 5, 3)};
        EXPECT_FALSE(interval.contains(day(2026, 4, 30)));
        EXPECT_TRUE(interval.contains(day(2026, 5, 1)));
        EXPECT_TRUE(interval.contains(day(2026, 5, 2)));
        EXPECT_TRUE(interval.contains(day(2026, 5, 3)));
        EXPECT_FALSE(interval.contains(day(2026, 5, 4)));
    }

    TEST(SelectRows, DateIntervalKeepsOriginalIndicesIncludingBothEndpointDays) {
        const std::array days{day(2026, 5, 1), day(2026, 5, 1), day(2026, 5, 2), day(2026, 5, 4), day(2026, 5, 5)};
        const auto selected = selectRows(DayInterval{day(2026, 5, 1), day(2026, 5, 4)}, days);
        EXPECT_EQ(selected, (std::vector<std::size_t>{0, 1, 2, 3}));
        EXPECT_EQ(selectRows(DayInterval{day(2026, 5, 3), day(2026, 5, 3)}, days), std::vector<std::size_t>{});
    }

    TEST(SelectRows, LatestRunsAreAChronologicalSuffix) {
        const std::array days{day(2026, 5, 1), day(2026, 5, 2), day(2026, 5, 3), day(2026, 5, 4)};
        EXPECT_EQ(selectRows(LatestRunSuffix{2}, days), (std::vector<std::size_t>{2, 3}));
    }

    TEST(SelectRows, LatestRunsLargerThanTheHistoryReturnEverything) {
        const std::array days{day(2026, 5, 1), day(2026, 5, 2)};
        EXPECT_EQ(selectRows(LatestRunSuffix{50}, days), (std::vector<std::size_t>{0, 1}));
    }

    TEST(SelectRows, LatestRunsWithNoRowsAreEmpty) {
        EXPECT_TRUE(selectRows(LatestRunSuffix{3}, std::span<const sys_days>{}).empty());
    }

    TEST(SelectRows, EntireHistorySelectsEveryRowAndNoRelevantHistorySelectsNone) {
        const std::array days{day(2026, 5, 1), day(2026, 5, 2), day(2026, 5, 3)};
        EXPECT_EQ(selectRows(EntireHistory{}, days), (std::vector<std::size_t>{0, 1, 2}));
        EXPECT_TRUE(selectRows(NoRelevantHistory{}, days).empty());
    }

    TEST(SelectRows, ReversedCustomIntervalSelectsNothing) {
        const std::array days{day(2026, 5, 2)};
        EXPECT_TRUE(selectRows(DayInterval{day(2026, 5, 3), day(2026, 5, 1)}, days).empty());
    }

    TEST(LatestDay, IsTheMaximumRegardlessOfOrderAndAbsentWhenEmpty) {
        const std::array days{day(2026, 5, 3), day(2026, 5, 9), day(2026, 5, 1)};
        EXPECT_EQ(latestDay(days), std::optional{day(2026, 5, 9)});
        EXPECT_EQ(latestDay(std::span<const sys_days>{}), std::nullopt);
    }

    TEST(EpochDayOfMillis, MatchesScenarioRunIdStartDayAtTheUtcMidnightBoundary) {
        constexpr long long kMillisPerDay = 86'400'000LL;
        for (const long long millis: {0LL, kMillisPerDay - 1, kMillisPerDay, 1'760'000'000'123LL, -1LL,
                                      -kMillisPerDay}) {
            ksv::domain::ScenarioRunId id;
            id.start_time = millis;
            EXPECT_EQ(epochDayOfMillis(millis), id.startDay()) << millis;
        }
    }

    TEST(DayRollover, SettingTheSameDayAnnouncesNothing) {
        ksv::tests_support::FakeDaySource source{kToday};
        int announcements = 0;
        source.onDayChanged([&](sys_days) { ++announcements; });
        source.setToday(kToday);
        EXPECT_EQ(announcements, 0);
    }
}
