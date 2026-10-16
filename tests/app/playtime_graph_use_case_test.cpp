//
// PlaytimeGraphUseCase tests over the shared FakeProfileService.
//

#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include "usecases/playtime_graph_use_case.h"

#include "fake_day_source.h"
#include "fake_graph_configuration_use_case.h"
#include "fake_profile_service.h"
#include "run_builders.h"
#include "user_profile.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    constexpr long long kMsPerDay = 86'400'000LL;

    std::chrono::sys_days day(const long long days_since_epoch) {
        return std::chrono::sys_days{} + std::chrono::days{days_since_epoch};
    }

    RangePreference firstThreeMonthsOf1970() {
        return RangePreference{
            CustomDates{std::chrono::year{1970} / 1 / 1, std::chrono::year{1970} / 4 / 1}, false};
    }

    class PlaytimeGraphUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();
        std::shared_ptr<FakeDaySource> days = std::make_shared<FakeDaySource>();
        PlaytimeGraphUseCase use_case{profile, configuration, days};
        int notifications = 0;
        int styleNotifications = 0;

        PlaytimeGraphUseCaseTest() {
            use_case.onChanged([this] { ++notifications; });
            use_case.onPresentationChanged([this] { ++styleNotifications; });
        }

        GraphPreferences &preferences() { return configuration->configuration.preferences(GraphKey::DailyPlaytime); }

        void commit(const GraphKey key = GraphKey::DailyPlaytime) {
            ++configuration->revision;
            configuration->publish({.graphs = {key}});
        }

        // Real accounting behind the fake, so windows change values the way the profile would.
        void useRealAccounting(const std::vector<std::pair<long long, float> > &dailyRuns) {
            auto realProfile = std::make_shared<UserProfile>();
            for (const auto &[epochDay, seconds]: dailyRuns)
                realProfile->addRun(makeRun("s", epochDay * kMsPerDay, 0.0F, 0, 0, seconds));
            profile->rolling_by_window = [realProfile](const int window) {
                return realProfile->getRollingTimeAverage(window);
            };
            profile->latest_run = *realProfile->getLatestRun();
        }
    };

    TEST_F(PlaytimeGraphUseCaseTest, DefaultsToThreeDayAveragingOverAllHistory) {
        profile->rolling_time_average = {{day(19500), 1800.0}, {day(19501), 2400.0}};

        const auto snapshot = use_case.snapshot();

        EXPECT_EQ(profile->rolling_time_average_window_days, 3);
        EXPECT_EQ(snapshot.view.windowDays, 3);
        EXPECT_FALSE(snapshot.view.interval);
        EXPECT_EQ(snapshot.samples, profile->rolling_time_average);
    }

    TEST_F(PlaytimeGraphUseCaseTest, EmptyProfileSeriesProducesNoSamplesAndNoExtent) {
        const auto snapshot = use_case.snapshot();
        EXPECT_TRUE(snapshot.samples.empty());
        EXPECT_FALSE(snapshot.view.fullExtent);
    }

    TEST_F(PlaytimeGraphUseCaseTest, TheCommittedWindowIsWhatTheProfileIsAsked) {
        preferences().averagingWindowDays = 7;

        const auto snapshot = use_case.snapshot();

        EXPECT_EQ(profile->rolling_time_average_window_days, 7);
        EXPECT_EQ(snapshot.view.windowDays, 7);
    }

    TEST_F(PlaytimeGraphUseCaseTest, OneDayWindowYieldsTheDailyTotalsOfDaysWithRuns) {
        useRealAccounting({{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
        preferences().averagingWindowDays = 1;

        const auto snapshot = use_case.snapshot();

        EXPECT_EQ(snapshot.samples, (PlaytimeSeries{{day(100), 60.0}, {day(101), 30.0}, {day(104), 90.0}}));
    }

    TEST_F(PlaytimeGraphUseCaseTest, ARecentRangeShowsOnlyItsDaysAndKeepsTheirFullHistoryValues) {
        useRealAccounting({{100, 60.0F}, {101, 30.0F}, {104, 90.0F}, {106, 30.0F}});
        const auto before = use_case.snapshot();
        days->setToday(day(106));
        preferences().range = RangePreference{RecentDays{3}, false};

        const auto narrowed = use_case.snapshot();

        ASSERT_EQ(narrowed.samples.size(), 3U);
        ASSERT_GE(before.samples.size(), 3U);
        const auto overlap = std::vector(before.samples.end() - 3, before.samples.end());
        EXPECT_EQ(narrowed.samples, overlap);
        // The window reaches back before the range, so the last value still counts day 104.
        EXPECT_DOUBLE_EQ(narrowed.samples.back().second, (90.0 + 30.0) / 3.0);
    }

    TEST_F(PlaytimeGraphUseCaseTest, TheIntervalIncludesBlankDaysAndNeverInventsSamplesPastTheHistory) {
        useRealAccounting({{100, 60.0F}, {101, 30.0F}});
        days->setToday(day(110));
        preferences().range = RangePreference{RecentDays{5}, false};

        const auto snapshot = use_case.snapshot();

        EXPECT_EQ(snapshot.view.interval, std::optional(DayInterval{day(106), day(110)}));
        EXPECT_TRUE(snapshot.samples.empty());
    }

    TEST_F(PlaytimeGraphUseCaseTest, AnchoringUsesTheNewestRunOfTheWholeProfile) {
        useRealAccounting({{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
        days->setToday(day(300));
        preferences().range = RangePreference{RecentDays{2}, true};

        const auto snapshot = use_case.snapshot();

        EXPECT_EQ(snapshot.view.interval, std::optional(DayInterval{day(103), day(104)}));
        ASSERT_EQ(snapshot.samples.size(), 2U);
        EXPECT_EQ(snapshot.samples.front().first, day(103));
        EXPECT_EQ(snapshot.samples.back().first, day(104));
    }

    TEST_F(PlaytimeGraphUseCaseTest, AnchoredRangeWithNoRunsIsEmptyRatherThanBorrowingToday) {
        preferences().range = RangePreference{RecentDays{7}, true};

        const auto snapshot = use_case.snapshot();

        EXPECT_FALSE(snapshot.view.interval);
        EXPECT_TRUE(snapshot.samples.empty());
    }

    TEST_F(PlaytimeGraphUseCaseTest, ChangingOnlyTheRangeKeepsTheWindowAndRecalculatesNothing) {
        useRealAccounting({{100, 60.0F}, {101, 30.0F}});
        preferences().averagingWindowDays = 5;
        std::ignore = use_case.snapshot();
        const auto requests = profile->rolling_time_average_request_count;

        preferences().range = firstThreeMonthsOf1970();
        commit();
        const auto snapshot = use_case.snapshot();

        EXPECT_EQ(snapshot.view.windowDays, 5);
        EXPECT_EQ(profile->rolling_time_average_request_count, requests);
        EXPECT_EQ(notifications, 1);
    }

    TEST_F(PlaytimeGraphUseCaseTest, ChangingTheWindowRecalculatesOnceAndLeavesTheRangeAlone) {
        useRealAccounting({{100, 60.0F}, {101, 30.0F}});
        preferences().range = firstThreeMonthsOf1970();
        std::ignore = use_case.snapshot();
        const auto requests = profile->rolling_time_average_request_count;

        preferences().averagingWindowDays = 9;
        commit();
        const auto first = use_case.snapshot();
        const auto second = use_case.snapshot();

        EXPECT_EQ(profile->rolling_time_average_request_count, requests + 1);
        EXPECT_EQ(first.view.windowDays, 9);
        EXPECT_EQ(first.view.interval, second.view.interval);
        EXPECT_TRUE(first.view.interval);
    }

    TEST_F(PlaytimeGraphUseCaseTest, StyleOnlyChangesNeverRecalculateOrRefreshTheData) {
        useRealAccounting({{100, 60.0F}});
        std::ignore = use_case.snapshot();
        const auto requests = profile->rolling_time_average_request_count;

        preferences().appearance.connection = ConnectionMode::Straight;
        preferences().seriesStyles["playtime"] = {RgbaColor{1, 2, 3, 255}, 5.0};
        preferences().axisPolicies["playtime"] = FitVisibleData{};
        configuration->publishDiff();
        std::ignore = use_case.snapshot();

        EXPECT_EQ(profile->rolling_time_average_request_count, requests);
        EXPECT_EQ(notifications, 0);
        EXPECT_EQ(styleNotifications, 1);
        const auto style = use_case.presentation();
        EXPECT_EQ(style.appearance.connection, ConnectionMode::Straight);
        EXPECT_EQ(style.seriesStyle("playtime"), (ResolvedSeriesStyle{RgbaColor{1, 2, 3, 255}, 5.0}));
        EXPECT_EQ(style.axisPolicies.at("playtime"), AxisPolicy{FitVisibleData{}});
    }

    TEST_F(PlaytimeGraphUseCaseTest, RangeAndWindowChangesRefreshTheDataButNeverTheStyle) {
        useRealAccounting({{100, 60.0F}});
        preferences().range = firstThreeMonthsOf1970();
        configuration->publishDiff();
        preferences().averagingWindowDays = 6;
        configuration->publishDiff();

        EXPECT_EQ(notifications, 2);
        EXPECT_EQ(styleNotifications, 0);
    }

    TEST_F(PlaytimeGraphUseCaseTest, ARangeAndAStyleEditInOneSaveTakeBothPaths) {
        preferences().range = firstThreeMonthsOf1970();
        preferences().appearance.connection = ConnectionMode::PointsOnly;
        configuration->publishDiff();

        EXPECT_EQ(notifications, 1);
        EXPECT_EQ(styleNotifications, 1);
    }

    TEST_F(PlaytimeGraphUseCaseTest, TheStyleIsTheDailyPlaytimeGraphsWhateverTheProfileHolds) {
        configuration->configuration.preferences(GraphKey::BenchmarkPlaytime).appearance.connection =
            ConnectionMode::Straight;
        configuration->publishDiff();
        profile->notifyProfileChanged();

        EXPECT_EQ(styleNotifications, 0);
        EXPECT_EQ(use_case.presentation(), defaultGraphPresentation(GraphKey::DailyPlaytime));
    }

    TEST_F(PlaytimeGraphUseCaseTest, OtherGraphsChangesAreIgnored) {
        commit(GraphKey::BenchmarkPlaytime);
        commit(GraphKey::ScenarioHistory);
        EXPECT_EQ(notifications, 0);
    }

    TEST_F(PlaytimeGraphUseCaseTest, ANewProfileRecalculatesAndNotifies) {
        useRealAccounting({{100, 60.0F}});
        std::ignore = use_case.snapshot();
        const auto requests = profile->rolling_time_average_request_count;

        profile->notifyProfileChanged();
        std::ignore = use_case.snapshot();

        EXPECT_EQ(notifications, 1);
        EXPECT_EQ(profile->rolling_time_average_request_count, requests + 1);
    }

    TEST_F(PlaytimeGraphUseCaseTest, ALaterRunMovesTheLatestRunAnchoredIntervalOnceTheProfileChanges) {
        useRealAccounting({{100, 60.0F}, {104, 90.0F}});
        days->setToday(day(300));
        preferences().range = RangePreference{RecentDays{2}, true};
        const auto before = use_case.snapshot();
        ASSERT_EQ(before.view.interval, std::optional(DayInterval{day(103), day(104)}));

        useRealAccounting({{100, 60.0F}, {104, 90.0F}, {108, 30.0F}});
        profile->notifyProfileChanged();
        const auto after = use_case.snapshot();

        EXPECT_EQ(notifications, 1);
        EXPECT_EQ(after.view.interval, std::optional(DayInterval{day(107), day(108)}));
        ASSERT_FALSE(after.samples.empty());
        EXPECT_EQ(after.samples.back().first, day(108));
    }

    TEST_F(PlaytimeGraphUseCaseTest, TheAnchorIsOnlyResolvedAgainAfterTheProfileChanges) {
        useRealAccounting({{100, 60.0F}, {104, 90.0F}});
        days->setToday(day(300));
        preferences().range = RangePreference{RecentDays{2}, true};
        std::ignore = use_case.snapshot();

        useRealAccounting({{100, 60.0F}, {104, 90.0F}, {108, 30.0F}});
        EXPECT_EQ(use_case.snapshot().view.interval, std::optional(DayInterval{day(103), day(104)}))
            << "the anchor is kept per profile revision, so no change notification means no new anchor";

        profile->notifyProfileChanged();
        EXPECT_EQ(use_case.snapshot().view.interval, std::optional(DayInterval{day(107), day(108)}));
    }

    TEST_F(PlaytimeGraphUseCaseTest, ADayChangeReprojectsATodayAnchoredRangeWithoutRecalculating) {
        useRealAccounting({{100, 60.0F}, {101, 30.0F}});
        days->setToday(day(101));
        preferences().range = RangePreference{RecentDays{2}, false};
        const auto before = use_case.snapshot();
        const auto requests = profile->rolling_time_average_request_count;

        days->advance();
        const auto after = use_case.snapshot();

        EXPECT_EQ(notifications, 1);
        EXPECT_EQ(before.view.interval, std::optional(DayInterval{day(100), day(101)}));
        EXPECT_EQ(after.view.interval, std::optional(DayInterval{day(101), day(102)}));
        EXPECT_EQ(after.samples.size(), 1U);
        EXPECT_EQ(profile->rolling_time_average_request_count, requests);
    }

    TEST_F(PlaytimeGraphUseCaseTest, ADayChangeDoesNotDisturbRangesThatDoNotFollowToday) {
        preferences().range = RangePreference{RecentDays{2}, true};
        days->advance();
        preferences().range = RangePreference{};
        days->advance();
        EXPECT_EQ(notifications, 0);
    }
}
