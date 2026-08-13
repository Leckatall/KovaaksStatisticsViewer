#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "usecases/completion_history_use_case.h"

using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSessionController final : public ISessionController {
    public:
        ScenarioPerf current_perf;

        std::vector<ScenarioId> getScenarioList() override { return {}; }
        void generateProfileFromDirectory() override {}
        void setCurrentPerfToLatest() override {}
        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }
        void setCurrentPerf(const std::string &) override {}
        void setCurrentPerf(const ScenarioRunId &) override {}
        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }
        [[nodiscard]] bool isBuildInProgress() const override { return false; }
        [[nodiscard]] std::vector<ScenarioSummary> getScenarioSummaries() const override { return {}; }
        [[nodiscard]] std::vector<RunSummary> getRunsForScenario(const ScenarioId &) const override { return {}; }
        [[nodiscard]] std::vector<RunSummary> getRecentRuns(std::size_t) const override { return {}; }

        void changeRun(const std::string &hash, const long long start_time) {
            current_perf.run_id.scenario_id = {.name = "Scenario " + hash, .hash = hash};
            current_perf.run_id.start_time = start_time;
            emit currentPerfChanged();
        }

        void notifyProfileChanged() { emit profileChanged(); }
    };

    class FakeProfileService final : public IProfileService {
    public:
        mutable int completion_history_calls = 0;
        mutable ScenarioId requested_scenario;
        std::vector<std::pair<ScenarioRunId, ScenarioCompletionData> > completion_history;

        void generateProfileFromDirectory() override {}
        void loadProfile() override {}
        void onBuildRequested(std::function<void()>) override {}
        void beginProfileBuild() override {}
        void applyBuiltProfile(UserProfile) override {}
        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const override { return {}; }
        [[nodiscard]] ScenarioPerf getPerf(const std::string &) const override { return {}; }
        [[nodiscard]] ScenarioPerf getLatestPerf() const override { return {}; }
        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(const ScenarioId &, std::size_t) const override {
            return {};
        }
        [[nodiscard]] std::vector<std::pair<ScenarioRunId, ScenarioCompletionData> >
        getCompletionHistory(const ScenarioId &scenario) const override {
            ++completion_history_calls;
            requested_scenario = scenario;
            return completion_history;
        }
        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &, std::size_t) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::optional<ScenarioPerf> getRun(const ScenarioRunId &) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::optional<std::size_t> getRunCount(const ScenarioId &) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::optional<std::chrono::sys_seconds> getLastRunTime(const ScenarioId &) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::optional<double> getTotalTime(const ScenarioId &) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::vector<ScenarioPerf> getRecentRuns(std::size_t) const override { return {}; }
        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(int) const override { return {}; }
        [[nodiscard]] bool isProfileLoaded() const override { return true; }
        void onProfileChanged(std::function<void()>) override {}
    };

    class CompletionHistoryUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> session = std::make_shared<FakeSessionController>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        CompletionHistoryUseCase use_case{session, profile};

        void setCurrentScenario(const std::string &hash = "scenario-1", const long long start_time = 300) const {
            session->current_perf.run_id.scenario_id = {.name = "Scenario One", .hash = hash};
            session->current_perf.run_id.start_time = start_time;
        }
    };

    TEST_F(CompletionHistoryUseCaseTest, MapsCompletionRowsAndAssignsOneBasedRunIndices) {
        setCurrentScenario();
        profile->completion_history = {
            std::make_pair(
                ScenarioRunId{.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100},
                ScenarioCompletionData{.shots = 10, .hits = 4, .misses = 6, .score = 25.0F}),
            std::make_pair(
                ScenarioRunId{.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 200},
                ScenarioCompletionData{.shots = 20, .hits = 15, .misses = 5, .score = 75.0F}),
        };

        const auto history = use_case.get_history();

        EXPECT_EQ(profile->requested_scenario.hash, "scenario-1");
        EXPECT_EQ(history.scenario_name, "Scenario One");
        ASSERT_EQ(history.rows.size(), 2);
        EXPECT_EQ(history.rows[0].run_index, 1);
        EXPECT_EQ(history.rows[0].start_time_ms, 100);
        EXPECT_DOUBLE_EQ(history.rows[0].score, 25.0);
        EXPECT_DOUBLE_EQ(history.rows[0].accuracy, 0.4);
        EXPECT_DOUBLE_EQ(history.rows[0].shots, 10.0);
        EXPECT_DOUBLE_EQ(history.rows[0].hits, 4.0);
        EXPECT_DOUBLE_EQ(history.rows[0].misses, 6.0);
        EXPECT_EQ(history.rows[1].run_index, 2);
    }

    TEST_F(CompletionHistoryUseCaseTest, AccuracyIsZeroWhenShotsAreZero) {
        setCurrentScenario();
        profile->completion_history = {
            std::make_pair(
                ScenarioRunId{.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100},
                ScenarioCompletionData{.shots = 0, .hits = 0}),
        };

        const auto history = use_case.get_history();

        ASSERT_EQ(history.rows.size(), 1);
        EXPECT_DOUBLE_EQ(history.rows[0].accuracy, 0.0);
    }

    TEST_F(CompletionHistoryUseCaseTest, EmptyCurrentScenarioDoesNotQueryProfile) {
        const auto history = use_case.get_history();

        EXPECT_TRUE(history.rows.empty());
        EXPECT_EQ(profile->completion_history_calls, 0);
    }

    TEST_F(CompletionHistoryUseCaseTest, CallbackOnlyFiresWhenScenarioHashChanges) {
        setCurrentScenario("scenario-1", 100);
        int callback_count = 0;
        use_case.onCurrentScenarioChanged([&callback_count] { ++callback_count; });

        session->changeRun("scenario-1", 200);
        EXPECT_EQ(callback_count, 0);

        session->changeRun("scenario-2", 300);
        EXPECT_EQ(callback_count, 1);
    }

    TEST_F(CompletionHistoryUseCaseTest, ProfileChangeAlwaysFiresAndRestoresTheScenarioGate) {
        setCurrentScenario("scenario-1", 100);
        int callback_count = 0;
        use_case.onCurrentScenarioChanged([&callback_count] { ++callback_count; });

        session->notifyProfileChanged();
        EXPECT_EQ(callback_count, 1);

        session->changeRun("scenario-1", 200);
        EXPECT_EQ(callback_count, 1);
    }
}
