//
// PlaytimeGraphUseCase tests using a hand-written fake IProfileService.
//

#include <gtest/gtest.h>

#include <chrono>

#include "usecases/playtime_graph_use_case.h"

using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakePlaytimeProfileService : public IProfileService {
    public:
        std::vector<std::pair<std::chrono::sys_days, double>> rolling;
        int last_window_days = 0;

        void generateProfileFromDirectory() override {}
        void loadProfile() override {}
        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const override { return {}; }
        [[nodiscard]] ScenarioPerf getPerf(const std::string &) const override { return {}; }
        [[nodiscard]] ScenarioPerf getLatestPerf() const override { return {}; }
        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &, std::size_t) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double>>
        getRollingTimeAverage(const int window_days) const override {
            const_cast<FakePlaytimeProfileService *>(this)->last_window_days = window_days;
            return rolling;
        }
        [[nodiscard]] bool isProfileLoaded() const override { return true; }
        void onProfileChanged(std::function<void()>) override {}
    };

    std::chrono::sys_days day(const long long days_since_epoch) {
        return std::chrono::sys_days{} + std::chrono::days{days_since_epoch};
    }

    class PlaytimeGraphUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakePlaytimeProfileService> fake = std::make_shared<FakePlaytimeProfileService>();
        PlaytimeGraphUseCase use_case{fake};
    };

    TEST_F(PlaytimeGraphUseCaseTest, PassesWindowDaysThroughToProfile) {
        use_case.get_rolling_playtime(3);
        EXPECT_EQ(fake->last_window_days, 3);
    }

    TEST_F(PlaytimeGraphUseCaseTest, MapsSysDaysToDaysSinceEpochPreservingSeconds) {
        fake->rolling = {{day(19500), 1800.0}, {day(19501), 2400.0}};

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
