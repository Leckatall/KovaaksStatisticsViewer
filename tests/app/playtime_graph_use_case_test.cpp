//
// PlaytimeGraphUseCase tests over the shared FakeProfileService.
//

#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include "usecases/playtime_graph_use_case.h"

#include "fake_profile_service.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    std::chrono::sys_days day(const long long days_since_epoch) {
        return std::chrono::sys_days{} + std::chrono::days{days_since_epoch};
    }

    class PlaytimeGraphUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeProfileService> fake = std::make_shared<FakeProfileService>();
        PlaytimeGraphUseCase use_case{fake};
    };

    TEST_F(PlaytimeGraphUseCaseTest, PassesWindowDaysThroughToProfile) {
        use_case.get_rolling_playtime(3);
        EXPECT_EQ(fake->rolling_time_average_window_days, 3);
    }

    TEST_F(PlaytimeGraphUseCaseTest, MapsSysDaysToDaysSinceEpochPreservingSeconds) {
        fake->rolling_time_average = {{day(19500), 1800.0}, {day(19501), 2400.0}};

        const auto result = use_case.get_rolling_playtime(3);

        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0].first, 19500);
        EXPECT_DOUBLE_EQ(result[0].second, 1800.0);
        EXPECT_EQ(result[1].first, 19501);
        EXPECT_DOUBLE_EQ(result[1].second, 2400.0);
    }

    TEST_F(PlaytimeGraphUseCaseTest, EmptyProfileSeriesProducesEmptyResult) {
        EXPECT_TRUE(use_case.get_rolling_playtime(3).empty());
    }
}
