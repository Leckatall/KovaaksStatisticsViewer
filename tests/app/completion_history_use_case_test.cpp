#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "usecases/completion_history_use_case.h"
#include "fake_profile_service.h"
#include "fake_session_controller.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    class CompletionHistoryUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> session = std::make_shared<FakeSessionController>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        CompletionHistoryUseCase use_case{session, profile};

        void setCurrentScenario(const std::string &hash = "scenario-1", const long long start_time = 300) const {
            session->current_run.run_id.scenario_id = {.name = "Scenario One", .hash = hash};
            session->current_run.run_id.start_time = start_time;
        }
    };

    TEST_F(CompletionHistoryUseCaseTest, MapsCompletionRowsAndAssignsOneBasedRunIndices) {
        setCurrentScenario();
        profile->completion_history = {
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100},
             .totals = {.score = 25.0F, .shots = 10, .hits = 4, .misses = 6}},
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 200},
             .totals = {.score = 75.0F, .shots = 20, .hits = 15, .misses = 5}},
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
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100},
             .totals = {.shots = 0, .hits = 0}},
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
