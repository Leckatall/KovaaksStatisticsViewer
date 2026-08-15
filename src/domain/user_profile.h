//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_USER_PROFILE_H
#define KOVAAKSSTATSVIEWER_USER_PROFILE_H
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "scenario_perf.h"
#include "source_directory.h"

namespace ksv::domain {
    struct ScenarioAggregate {
        double total_time_seconds = 0.0;
        std::size_t run_count = 0;
    };

    class UserProfile {
    public:
        explicit UserProfile(SourceRegistry sources = {});

        bool addScenarioPerf(const ScenarioPerf &perf);

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const;

        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf() const;

        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &scenario) const;

        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(const ScenarioId &scenario, std::size_t count) const;

        [[nodiscard]] std::vector<RunData> getCompletionHistory(const ScenarioId &scenario) const;

        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &scenario, std::size_t count) const;

        [[nodiscard]] std::optional<ScenarioPerf> getRun(const ScenarioRunId &run_id) const;

        [[nodiscard]] std::optional<std::chrono::sys_seconds> getLastRunTime(const ScenarioId &scenario) const;

        [[nodiscard]] std::optional<double> getTotalTime(const ScenarioId &scenario) const;

        [[nodiscard]] double getTotalTimeAllScenarios() const;

        [[nodiscard]] std::optional<std::size_t> getRunCount(const ScenarioId &scenario) const;

        [[nodiscard]] const SourceRegistry &sources() const;
        DirectoryId ensureSource(const std::string &root, const std::string &subdir);

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(const ScenarioId &scenario, int window_days) const;

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(int window_days) const;

        [[nodiscard]] const std::vector<ScenarioPerf> &getAllRunRecords() const;

    private:
        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        rollingTimeAverageFor(const std::vector<std::size_t> &sorted_indices, int window_days) const;

        SourceRegistry m_sources;
        // Append-only; indices stable and used as keys in maps below
        std::vector<ScenarioPerf> m_runs;
        std::unordered_map<ScenarioRunId, std::size_t> m_run_index;
        // Each scenario's index list is kept sorted ascending by run_id.start_time.
        std::unordered_map<ScenarioId, std::vector<std::size_t> > m_scenario_index;
        std::unordered_map<ScenarioId, ScenarioAggregate> m_scenario_aggregate;
        double m_total_time_all_scenarios = 0.0;
        std::optional<std::size_t> m_most_recent_index;
    };
}

#endif //KOVAAKSSTATSVIEWER_USER_PROFILE_H
