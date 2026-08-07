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

namespace ksv::domain {
    struct ScenarioAggregate {
        double total_time_seconds = 0.0;
        std::size_t run_count = 0;
    };

    class UserProfile {
    public:
        explicit UserProfile(std::string source_directory = {});

        // Inserts a run. ScenarioRunId is a primary key: if a run with the same
        // run_id already exists, the insert is skipped (and logged) and false is
        // returned. Returns true if the run was inserted.
        bool addScenarioPerf(const ScenarioPerf &perf);

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const;

        // Most recent run across all scenarios, by start_time.
        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf() const;

        // Most recent run for a specific scenario.
        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &scenario) const;

        // Up to `count` most recent runs for a scenario, oldest-first.
        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(const ScenarioId &scenario, std::size_t count) const;

        // Average final score across the `count` most recent runs for a scenario.
        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &scenario, std::size_t count) const;

        // Lookup a specific run by its primary key.
        [[nodiscard]] std::optional<ScenarioPerf> getRun(const ScenarioRunId &run_id) const;

        // Sum of scenario_length across all runs of a scenario.
        [[nodiscard]] std::optional<double> getTotalTime(const ScenarioId &scenario) const;

        // Sum of scenario_length across all runs of all scenarios.
        [[nodiscard]] double getTotalTimeAllScenarios() const;

        [[nodiscard]] std::optional<std::size_t> getRunCount(const ScenarioId &scenario) const;

        [[nodiscard]] const std::string &getSourceDirectory() const;

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(const ScenarioId &scenario, int window_days) const;

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(int window_days) const;

        [[nodiscard]] const std::vector<ScenarioPerf> &getAllRunRecords() const;

    private:
        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        rollingTimeAverageFor(const std::vector<std::size_t> &sorted_indices, int window_days) const;

        std::string m_source_directory;
        // Append-only. Indices into this vector are stable for its lifetime and
        // are what every index map below stores.
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
