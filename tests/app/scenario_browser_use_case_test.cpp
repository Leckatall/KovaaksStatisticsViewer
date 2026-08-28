#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include "usecases/scenario_browser_use_case.h"
#include "fake_profile_service.h"
#include "fake_session_controller.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    ksv::domain::Run makeRun(const std::string &hash, const long long start_time, const float score = 0.0F,
                          const int shots = 0, const int hits = 0, const float duration = 0.0F) {
        ksv::domain::Run perf;
        perf.run_id.scenario_id = {.name = "Scenario " + hash, .hash = hash};
        perf.run_id.start_time = start_time;
        perf.scenario_length = duration;
        perf.stored_totals = {.score = score, .shots = shots, .hits = hits, .misses = shots - hits};
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
            makeRun("hash-1", 100, 50.0F, 10, 5), makeRun("hash-1", 200, 75.0F, 20, 15)
        };

        const auto runs = use_case.getRunsForScenario(scenario);

        ASSERT_EQ(runs.size(), 2);
        EXPECT_EQ(runs[0].data.run_id.start_time, 200);
        EXPECT_FLOAT_EQ(runs[0].data.totals.score, 75.0F);
        EXPECT_DOUBLE_EQ(runs[0].data.totals.accuracy(), 0.75);
        EXPECT_EQ(runs[1].data.run_id.start_time, 100);
        EXPECT_TRUE(runs[0].personal_best);
        EXPECT_TRUE(runs[1].personal_best);
    }

    TEST_F(ScenarioBrowserUseCaseTest, MarksOnlyStrictScoreImprovementsAsPersonalBests) {
        const ScenarioId scenario{.name = "1wall6", .hash = "hash-1"};
        profile->run_counts[scenario] = 4;
        profile->perfs_by_scenario[scenario] = {
            makeRun("hash-1", 100, 50.0F),
            makeRun("hash-1", 200, 75.0F),
            makeRun("hash-1", 300, 75.0F),
            makeRun("hash-1", 400, 60.0F),
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
        profile->recent_runs = {makeRun("hash-1", 300, 75.0F, 20, 15, 45.0F)};

        const auto runs = use_case.getRecentRuns(5);

        ASSERT_EQ(runs.size(), 1);
        EXPECT_EQ(runs[0].data.run_id.scenario_id.name, "Scenario hash-1");
        EXPECT_EQ(runs[0].data.run_id.start_time, 300);
        EXPECT_FLOAT_EQ(runs[0].data.totals.score, 75.0F);
    }

    TEST_F(ScenarioBrowserUseCaseTest, MarksRecentRunsAgainstTheirOwnScenarioHistory) {
        const ScenarioId first{.name = "Scenario hash-1", .hash = "hash-1"};
        const ScenarioId second{.name = "Scenario hash-2", .hash = "hash-2"};
        const auto first_early = makeRun("hash-1", 100, 100.0F);
        const auto first_recent = makeRun("hash-1", 300, 90.0F);
        const auto second_early = makeRun("hash-2", 150, 10.0F);
        const auto second_recent = makeRun("hash-2", 200, 20.0F);
        profile->recent_runs = {
            makeRun("hash-1", 300, 90.0F),
            makeRun("hash-2", 200, 20.0F),
        };
        profile->completion_history_by_scenario[first] = {{first_early.run_id, first_early.totals()}, {first_recent.run_id, first_recent.totals()}};
        profile->completion_history_by_scenario[second] = {{second_early.run_id, second_early.totals()}, {second_recent.run_id, second_recent.totals()}};

        const auto runs = use_case.getRecentRuns(2);

        ASSERT_EQ(runs.size(), 2);
        EXPECT_EQ(runs[0].data.run_id.scenario_id.hash, "hash-1");
        EXPECT_FALSE(runs[0].personal_best);
        EXPECT_EQ(runs[1].data.run_id.scenario_id.hash, "hash-2");
        EXPECT_TRUE(runs[1].personal_best);
    }

    TEST_F(ScenarioBrowserUseCaseTest, DelegatesCurrentRunSelectionAndChangeNotifications) {
        session->current_run = makeRun("hash-1", 100);
        int changes = 0;
        use_case.onChanged([&changes] { ++changes; });

        const ScenarioRunId run_id{.scenario_id = {.name = "Scenario hash-2", .hash = "hash-2"}, .start_time = 200};
        use_case.selectRun(run_id);
        session->notifyChanged();

        EXPECT_EQ(use_case.getCurrentRun().run_id.start_time, 100);
        ASSERT_TRUE(session->selected_run.has_value());
        EXPECT_EQ(*session->selected_run, run_id);
        EXPECT_EQ(changes, 2);
    }
}
