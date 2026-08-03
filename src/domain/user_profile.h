//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_USER_PROFILE_H
#define KOVAAKSSTATSVIEWER_USER_PROFILE_H
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "scenario_perf.h"

namespace ksv::domain {
    class UserProfile {
    public:
        explicit UserProfile(std::string name);

        void addScenarioPerf(const ScenarioPerf &perf);

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const;

        // Most recent run across all scenarios, by start_time.
        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf() const;

        // Most recent run for a specific scenario.
        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &scenario) const;

        // Up to `count` most recent runs for a scenario, oldest-first (same order as storage).
        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(const ScenarioId &scenario, std::size_t count) const;

        // Average final score across the `count` most recent runs for a scenario.
        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &scenario, std::size_t count) const;

        // Full run history keyed by scenario, for persistence/serialization.
        [[nodiscard]] const std::map<ScenarioId, std::vector<ScenarioPerf> >& getAllRuns() const;

    private:
        std::string m_name;
        // std::string m_kovaaks_dir;
        // Each scenario's runs are kept sorted ascending by run_id.start_time.
        std::map<ScenarioId, std::vector<ScenarioPerf> > m_runs;
    };
}

#endif //KOVAAKSSTATSVIEWER_USER_PROFILE_H
