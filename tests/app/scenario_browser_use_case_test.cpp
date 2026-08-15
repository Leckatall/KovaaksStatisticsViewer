#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include "usecases/scenario_browser_use_case.h"

using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSessionController final : public ISessionController {
    public:
        ScenarioPerf current_perf;
        std::optional<ScenarioRunId> selected_run;

        std::vector<ScenarioId> getScenarioList() override { return {}; }
        void generateProfileFromDirectory() override {}
        void setCurrentPerfToLatest() override {}
        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }
        void setCurrentPerf(const std::string &) override {}
        void setCurrentPerf(const ScenarioRunId &run_id) override { selected_run = run_id; }
        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }
        [[nodiscard]] bool isBuildInProgress() const override { return false; }

        void notifyChanged() {
            emit currentPerfChanged();
            emit profileChanged();
        }
    };

    class FakeProfileService final : public IProfileService {
    public:
        std::vector<ScenarioId> scenarios;
        std::unordered_map<ScenarioId, std::vector<ScenarioPerf>> perfs_by_scenario;
        std::unordered_map<ScenarioId, std::vector<RunData>> completion_history_by_scenario;
        std::unordered_map<ScenarioId, std::size_t> run_counts;
        std::unordered_map<ScenarioId, double> total_times;
        std::vector<ScenarioPerf> recent_runs;

        void generateProfileFromDirectory() override {}
        void loadProfile() override {}
        void onBuildRequested(std::function<void()>) override {}
        void beginProfileBuild() override {}
        void applyBuiltProfile(UserProfile) override {}
        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const override { return scenarios; }
        [[nodiscard]] ScenarioPerf getPerf(const std::string &) const override { return {}; }
        [[nodiscard]] ScenarioPerf getLatestPerf() const override { return {}; }
        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &) const override { return std::nullopt; }
        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(const ScenarioId &scenario, std::size_t) const override {
            const auto it = perfs_by_scenario.find(scenario);
            return it == perfs_by_scenario.end() ? std::vector<ScenarioPerf>{} : it->second;
        }
        [[nodiscard]] std::vector<RunData> getCompletionHistory(const ScenarioId &scenario) const override {
            const auto it = completion_history_by_scenario.find(scenario);
            return it == completion_history_by_scenario.end() ? std::vector<RunData>{} : it->second;
        }
        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &, std::size_t) const override { return std::nullopt; }
        [[nodiscard]] std::optional<ScenarioPerf> getRun(const ScenarioRunId &) const override { return std::nullopt; }
        [[nodiscard]] std::optional<std::size_t> getRunCount(const ScenarioId &scenario) const override {
            const auto it = run_counts.find(scenario);
            return it == run_counts.end() ? std::nullopt : std::optional{it->second};
        }
        [[nodiscard]] std::optional<std::chrono::sys_seconds> getLastRunTime(const ScenarioId &) const override {
            return std::nullopt;
        }
        [[nodiscard]] std::optional<double> getTotalTime(const ScenarioId &scenario) const override {
            const auto it = total_times.find(scenario);
            return it == total_times.end() ? std::nullopt : std::optional{it->second};
        }
        [[nodiscard]] std::vector<ScenarioPerf> getRecentRuns(const std::size_t count) const override {
            auto result = recent_runs;
            if (result.size() > count) result.resize(count);
            return result;
        }
        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double>> getRollingTimeAverage(int) const override { return {}; }
        [[nodiscard]] bool isProfileLoaded() const override { return true; }
        void onProfileChanged(std::function<void()>) override {}
    };

    ScenarioPerf makePerf(const std::string &hash, const long long start_time, const float score = 0.0F,
                          const int shots = 0, const int hits = 0, const float duration = 0.0F) {
        ScenarioPerf perf;
        perf.run_id.scenario_id = {.name = "Scenario " + hash, .hash = hash};
        perf.run_id.start_time = start_time;
        perf.scenario_length = duration;
        perf.add_data(0.0F, SCORE, score);
        if (shots != 0) perf.add_data(0.0F, SHOTS, shots);
        if (hits != 0) perf.add_data(0.0F, HITS, hits);
        return perf;
    }

    class ScenarioBrowserUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> session = std::make_shared<FakeSessionController>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        ScenarioBrowserUseCase use_case{session, profile};
    };

    TEST_F(ScenarioBrowserUseCaseTest, CombinesScenarioListRunCountAndTotalTime) {
        const ScenarioId scenario{.name = "1wall6", .hash = "hash-1"};
        profile->scenarios = {scenario};
        profile->run_counts[scenario] = 3;
        profile->total_times[scenario] = 42.5;

        const auto summaries = use_case.getScenarioSummaries();

        ASSERT_EQ(summaries.size(), 1);
        EXPECT_EQ(summaries[0].scenario_id.hash, "hash-1");
        EXPECT_EQ(summaries[0].run_count, 3);
        EXPECT_DOUBLE_EQ(summaries[0].total_time_seconds, 42.5);
    }

    TEST_F(ScenarioBrowserUseCaseTest, MapsRunsAndReturnsThemNewestFirst) {
        const ScenarioId scenario{.name = "1wall6", .hash = "hash-1"};
        profile->run_counts[scenario] = 2;
        profile->perfs_by_scenario[scenario] = {
            makePerf("hash-1", 100, 50.0F, 10, 5), makePerf("hash-1", 200, 75.0F, 20, 15)
        };

        const auto runs = use_case.getRunsForScenario(scenario);

        ASSERT_EQ(runs.size(), 2);
        EXPECT_EQ(runs[0].data.run_id.start_time, 200);
        EXPECT_FLOAT_EQ(runs[0].data.score, 75.0F);
        EXPECT_DOUBLE_EQ(runs[0].data.accuracy(), 0.75);
        EXPECT_EQ(runs[1].data.run_id.start_time, 100);
        EXPECT_TRUE(runs[0].personal_best);
        EXPECT_TRUE(runs[1].personal_best);
    }

    TEST_F(ScenarioBrowserUseCaseTest, MarksOnlyStrictScoreImprovementsAsPersonalBests) {
        const ScenarioId scenario{.name = "1wall6", .hash = "hash-1"};
        profile->run_counts[scenario] = 4;
        profile->perfs_by_scenario[scenario] = {
            makePerf("hash-1", 100, 50.0F),
            makePerf("hash-1", 200, 75.0F),
            makePerf("hash-1", 300, 75.0F),
            makePerf("hash-1", 400, 60.0F),
        };

        const auto runs = use_case.getRunsForScenario(scenario);

        ASSERT_EQ(runs.size(), 4);
        EXPECT_EQ(runs[0].data.run_id.start_time, 400);
        EXPECT_FALSE(runs[0].personal_best);
        EXPECT_EQ(runs[1].data.run_id.start_time, 300);
        EXPECT_FALSE(runs[1].personal_best);
        EXPECT_EQ(runs[2].data.run_id.start_time, 200);
        EXPECT_TRUE(runs[2].personal_best);
        EXPECT_EQ(runs[3].data.run_id.start_time, 100);
        EXPECT_TRUE(runs[3].personal_best);
    }

    TEST_F(ScenarioBrowserUseCaseTest, MapsRecentRuns) {
        profile->recent_runs = {makePerf("hash-1", 300, 75.0F, 20, 15, 45.0F)};

        const auto runs = use_case.getRecentRuns(5);

        ASSERT_EQ(runs.size(), 1);
        EXPECT_EQ(runs[0].data.run_id.scenario_id.name, "Scenario hash-1");
        EXPECT_EQ(runs[0].data.run_id.start_time, 300);
        EXPECT_FLOAT_EQ(runs[0].data.score, 75.0F);
    }

    TEST_F(ScenarioBrowserUseCaseTest, MarksRecentRunsAgainstTheirOwnScenarioHistory) {
        const ScenarioId first{.name = "Scenario hash-1", .hash = "hash-1"};
        const ScenarioId second{.name = "Scenario hash-2", .hash = "hash-2"};
        const auto first_early = makePerf("hash-1", 100, 100.0F).getRunData();
        const auto first_recent = makePerf("hash-1", 300, 90.0F).getRunData();
        const auto second_early = makePerf("hash-2", 150, 10.0F).getRunData();
        const auto second_recent = makePerf("hash-2", 200, 20.0F).getRunData();
        profile->recent_runs = {
            makePerf("hash-1", 300, 90.0F),
            makePerf("hash-2", 200, 20.0F),
        };
        profile->completion_history_by_scenario[first] = {first_early, first_recent};
        profile->completion_history_by_scenario[second] = {second_early, second_recent};

        const auto runs = use_case.getRecentRuns(2);

        ASSERT_EQ(runs.size(), 2);
        EXPECT_EQ(runs[0].data.run_id.scenario_id.hash, "hash-1");
        EXPECT_FALSE(runs[0].personal_best);
        EXPECT_EQ(runs[1].data.run_id.scenario_id.hash, "hash-2");
        EXPECT_TRUE(runs[1].personal_best);
    }

    TEST_F(ScenarioBrowserUseCaseTest, DelegatesCurrentRunSelectionAndChangeNotifications) {
        session->current_perf = makePerf("hash-1", 100);
        int changes = 0;
        use_case.onChanged(session.get(), [&changes] { ++changes; });

        const ScenarioRunId run_id{.scenario_id = {.name = "Scenario hash-2", .hash = "hash-2"}, .start_time = 200};
        use_case.selectRun(run_id);
        session->notifyChanged();

        EXPECT_EQ(use_case.getCurrentPerf().run_id.start_time, 100);
        ASSERT_TRUE(session->selected_run.has_value());
        EXPECT_EQ(*session->selected_run, run_id);
        EXPECT_EQ(changes, 2);
    }
}
