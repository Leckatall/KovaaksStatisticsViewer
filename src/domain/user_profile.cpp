//
// Created by Lecka on 01/08/2026.
//

#include "user_profile.h"

#include <ranges>
#include <utility>

namespace ksv::domain {
    UserProfile::UserProfile(std::string name): m_name(std::move(name)) {
    }
    void UserProfile::addScenarioPerf(const ScenarioPerf& perf) {
        const ScenarioId scenario_hash = perf.run_id.scenario_id;
        if (!m_runs.contains(scenario_hash)) m_runs[scenario_hash] = std::vector<ScenarioPerf>{};
        m_runs.at(scenario_hash).push_back(perf);
    }

    std::vector<ScenarioId> UserProfile::getScenarioList() const {
        std::vector<ScenarioId> key_vector;
        for (const auto& k: m_runs | std::views::keys) {
            key_vector.push_back(k);
        }
        return key_vector;
    }
}
