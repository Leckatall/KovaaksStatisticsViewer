//
// Created by Lecka on 01/08/2026.
//

#include "user_profile.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace ksv::domain {
    UserProfile::UserProfile(std::string name) : m_name(std::move(name)) {
    }

    void UserProfile::addScenarioPerf(const ScenarioPerf &perf) {
        const ScenarioId scenario_hash = perf.run_id.scenario_id;
        auto &runs = m_runs[scenario_hash];
        const auto insert_pos = std::ranges::upper_bound(runs, perf,
                                                         [](const ScenarioPerf &a, const ScenarioPerf &b) {
                                                             return a.run_id.start_time < b.run_id.start_time;
                                                         });
        runs.insert(insert_pos, perf);
    }

    std::vector<ScenarioId> UserProfile::getScenarioList() const {
        std::vector<ScenarioId> key_vector;
        for (const auto &k: m_runs | std::views::keys) {
            key_vector.push_back(k);
        }
        return key_vector;
    }

    std::optional<ScenarioPerf> UserProfile::getMostRecentPerf() const {
        std::optional<ScenarioPerf> most_recent;
        for (const auto &runs: m_runs | std::views::values) {
            if (runs.empty()) continue;
            const auto &candidate = runs.back();
            if (!most_recent || candidate.run_id.start_time > most_recent->run_id.start_time) {
                most_recent = candidate;
            }
        }
        return most_recent;
    }

    std::optional<ScenarioPerf> UserProfile::getMostRecentPerf(const ScenarioId &scenario) const {
        const auto it = m_runs.find(scenario);
        if (it == m_runs.end() || it->second.empty()) return std::nullopt;
        return it->second.back();
    }

    std::vector<ScenarioPerf>
    UserProfile::getMostRecentPerfs(const ScenarioId &scenario, const std::size_t count) const {
        const auto it = m_runs.find(scenario);
        if (it == m_runs.end() || count == 0) return {};
        const auto &runs = it->second;
        const auto n = std::min(count, runs.size());
        return {runs.end() - static_cast<std::ptrdiff_t>(n), runs.end()};
    }

    std::optional<float> UserProfile::getAverageScore(const ScenarioId &scenario, const std::size_t count) const {
        const auto recent = getMostRecentPerfs(scenario, count);
        if (recent.empty()) return std::nullopt;

        float total = 0.0F;
        for (const auto &perf: recent) total += perf.getFinalScore();
        return total / static_cast<float>(recent.size());
    }

    const std::map<ScenarioId, std::vector<ScenarioPerf> >& UserProfile::getAllRuns() const {
        return m_runs;
    }
}
