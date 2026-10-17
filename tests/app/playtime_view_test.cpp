#include <gtest/gtest.h>

#include "app/contracts/playtime_view.h"

using namespace ksv::application;
using namespace std::chrono;

namespace {
    constexpr sys_days day(const int y, const unsigned m, const unsigned d) {
        return sys_days{year{y} / month{m} / std::chrono::day{d}};
    }

    const PlaytimeSeries kFull{{day(2026, 3, 1), 60.0}, {day(2026, 3, 2), 30.0}, {day(2026, 3, 5), 45.0},
                               {day(2026, 3, 6), 10.0}};

    TEST(PlaytimeView, UnsetPreferencesFallBackToThreeDaysOverAllHistory) {
        const GraphPreferences preferences;
        EXPECT_EQ(effectiveAveragingWindow(preferences), 3);
        EXPECT_EQ(effectiveRange(preferences), RangePreference{});
    }

    TEST(PlaytimeView, StoredPreferencesWinOverTheFallbacks) {
        GraphPreferences preferences;
        preferences.averagingWindowDays = 7;
        preferences.range = RangePreference{RecentDays{14}, true};
        EXPECT_EQ(effectiveAveragingWindow(preferences), 7);
        EXPECT_EQ(effectiveRange(preferences), (RangePreference{RecentDays{14}, true}));
    }

    TEST(VisiblePlaytime, EntireHistoryShowsEverySample) {
        EXPECT_EQ(visiblePlaytime(kFull, EntireHistory{}), kFull);
    }

    TEST(VisiblePlaytime, NoRelevantHistoryShowsNothing) {
        EXPECT_TRUE(visiblePlaytime(kFull, NoRelevantHistory{}).empty());
    }

    TEST(VisiblePlaytime, ADayIntervalKeepsBothEndpointsAndEachSamplesFullHistoryValue) {
        const auto visible = visiblePlaytime(kFull, DayInterval{day(2026, 3, 2), day(2026, 3, 5)});
        EXPECT_EQ(visible, (PlaytimeSeries{{day(2026, 3, 2), 30.0}, {day(2026, 3, 5), 45.0}}));
    }

    TEST(VisiblePlaytime, BlankDaysInsideOrAroundTheIntervalAddNoSamples) {
        const auto visible = visiblePlaytime(kFull, DayInterval{day(2026, 2, 1), day(2026, 3, 31)});
        EXPECT_EQ(visible, kFull);
        EXPECT_TRUE(visiblePlaytime(kFull, DayInterval{day(2026, 3, 3), day(2026, 3, 4)}).empty());
        EXPECT_TRUE(visiblePlaytime(kFull, DayInterval{day(2026, 4, 1), day(2026, 4, 30)}).empty());
    }

    TEST(VisiblePlaytime, ASingleDayIntervalIsThatDaysSample) {
        EXPECT_EQ(visiblePlaytime(kFull, DayInterval{day(2026, 3, 6), day(2026, 3, 6)}),
                  (PlaytimeSeries{{day(2026, 3, 6), 10.0}}));
    }

    TEST(VisiblePlaytime, ANarrowerIntervalIsASubsequenceOfAWiderOne) {
        const auto wide = visiblePlaytime(kFull, DayInterval{day(2026, 3, 1), day(2026, 3, 6)});
        const auto narrow = visiblePlaytime(kFull, DayInterval{day(2026, 3, 2), day(2026, 3, 5)});
        ASSERT_EQ(narrow.size(), 2U);
        EXPECT_EQ(narrow[0], wide[1]);
        EXPECT_EQ(narrow[1], wide[2]);
    }

    TEST(DescribePlaytimeView, CarriesTheWindowTheIntervalAndTheFullExtent) {
        const DayInterval interval{day(2026, 3, 2), day(2026, 3, 3)};
        const auto view = describePlaytimeView(7, interval, kFull);
        EXPECT_EQ(view.windowDays, 7);
        EXPECT_EQ(view.interval, std::optional{interval});
        const DayInterval extent{day(2026, 3, 1), day(2026, 3, 6)};
        EXPECT_EQ(view.fullExtent, std::optional{extent});
    }

    TEST(DescribePlaytimeView, EntireHistoryHasNoIntervalAndEmptyHistoryNoExtent) {
        const auto view = describePlaytimeView(3, EntireHistory{}, PlaytimeSeries{});
        EXPECT_FALSE(view.interval);
        EXPECT_FALSE(view.fullExtent);
    }

    TEST(FollowsToday, OnlyAnUnanchoredRecentPresetMovesWithTheCalendar) {
        EXPECT_TRUE(followsToday({RecentDays{7}, false}));
        EXPECT_FALSE(followsToday({RecentDays{7}, true}));
        EXPECT_FALSE(followsToday({AllHistory{}, false}));
        EXPECT_FALSE(followsToday({CustomDates{year{2026} / 3 / 1, year{2026} / 3 / 2}, false}));
    }
}
