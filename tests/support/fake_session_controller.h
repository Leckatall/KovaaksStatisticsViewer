#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_SESSION_CONTROLLER_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_SESSION_CONTROLLER_H

#include <optional>
#include <string>
#include <vector>

#include "usecases/i_session_controller.h"

namespace ksv::tests_support {
    class FakeSessionController final : public application::ISessionController {
    public:
        std::vector<domain::ScenarioId> scenario_list;
        domain::Run current_run;
        std::optional<domain::ScenarioRunId> selected_run;
        std::vector<std::string> set_current_perf_filename_calls;
        int generate_call_count = 0;
        bool build_in_progress = false;

        std::vector<domain::ScenarioId> getScenarioList() override { return scenario_list; }
        void generateProfileFromDirectory() override { ++generate_call_count; }
        void setCurrentRunToLatest() override {}
        void setCurrentRun(const domain::Run &run) override { current_run = run; }
        void setCurrentPerf(const std::string &filename) override { set_current_perf_filename_calls.push_back(filename); }
        void setCurrentRun(const domain::ScenarioRunId &run_id) override { selected_run = run_id; }
        [[nodiscard]] domain::Run getCurrentRun() const override { return current_run; }
        [[nodiscard]] bool isBuildInProgress() const override { return build_in_progress; }

        void changeRun(const std::string &hash, const long long start_time) {
            current_run.run_id.scenario_id = {.name = "Scenario " + hash, .hash = hash};
            current_run.run_id.start_time = start_time;
            emit currentRunChanged();
        }

        void notifyProfileChanged() { emit profileChanged(); }

        void notifyChanged() {
            emit currentRunChanged();
            emit profileChanged();
        }
    };
}

#endif
