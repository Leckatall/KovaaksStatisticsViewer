//
// Created by Lecka on 01/08/2026.
//

#include "user_profile.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <ranges>
#include <utility>

namespace ksv::domain {
    UserProfile::UserProfile(std::string source_directory) : m_source_directory(std::move(source_directory)) {
    }

    bool UserProfile::addScenarioPerf(const ScenarioPerf &perf) {
        if (m_run_index.contains(perf.run_id)) {
            std::cerr << "Duplicate run_id for scenario '" << perf.run_id.scenario_id.name
                    << "' at start_time " << perf.run_id.start_time << ", skipping." << std::endl;
            return false;
        }

        const std::size_t index = m_runs.size();
        m_runs.push_back(perf);
        m_run_index.emplace(perf.run_id, index);

        const ScenarioId &scenario = perf.run_id.scenario_id;
        auto &indices = m_scenario_index[scenario];
        const auto insert_pos = std::ranges::upper_bound(indices, perf.run_id.start_time, {},
                                                           [this](const std::size_t idx) {
                                                               return m_runs[idx].run_id.start_time;
                                                           });
        indices.insert(insert_pos, index);

        auto &aggregate = m_scenario_aggregate[scenario];
        aggregate.total_time_seconds += perf.scenario_length;
        aggregate.run_count += 1;
        m_total_time_all_scenarios += perf.scenario_length;

        if (!m_most_recent_index || perf.run_id.start_time > m_runs[*m_most_recent_index].run_id.start_time) {
            m_most_recent_index = index;
        }

        return true;
    }

    std::vector<ScenarioId> UserProfile::getScenarioList() const {
        std::vector<ScenarioId> keys;
        keys.reserve(m_scenario_index.size());
        for (const auto &scenario: m_scenario_index | std::views::keys) {
            keys.push_back(scenario);
        }
        return keys;
    }

    std::optional<ScenarioPerf> UserProfile::getMostRecentPerf() const {
        if (!m_most_recent_index) return std::nullopt;
        return m_runs[*m_most_recent_index];
    }

    std::optional<ScenarioPerf> UserProfile::getMostRecentPerf(const ScenarioId &scenario) const {
        const auto it = m_scenario_index.find(scenario);
        if (it == m_scenario_index.end() || it->second.empty()) return std::nullopt;
        return m_runs[it->second.back()];
    }

    std::vector<ScenarioPerf>
    UserProfile::getMostRecentPerfs(const ScenarioId &scenario, const std::size_t count) const {
        const auto it = m_scenario_index.find(scenario);
        if (it == m_scenario_index.end() || count == 0) return {};
        const auto &indices = it->second;
        const auto n = std::min(count, indices.size());

        std::vector<ScenarioPerf> result;
        result.reserve(n);
        for (auto idx_it = indices.end() - static_cast<std::ptrdiff_t>(n); idx_it != indices.end(); ++idx_it) {
            result.push_back(m_runs[*idx_it]);
        }
        return result;
    }

    std::vector<std::pair<ScenarioRunId, ScenarioCompletionData> >
    UserProfile::getCompletionHistory(const ScenarioId &scenario) const {
        const auto it = m_scenario_index.find(scenario);
        if (it == m_scenario_index.end()) return {};

        std::vector<std::pair<ScenarioRunId, ScenarioCompletionData> > result;
        result.reserve(it->second.size());
        for (const std::size_t idx: it->second) {
            const auto &perf = m_runs[idx];
            result.emplace_back(perf.run_id, perf.getCompletionData());
        }
        return result;
    }

    std::optional<float> UserProfile::getAverageScore(const ScenarioId &scenario, const std::size_t count) const {
        const auto recent = getMostRecentPerfs(scenario, count);
        if (recent.empty()) return std::nullopt;

        float total = 0.0F;
        for (const auto &perf: recent) total += perf.getCompletionData().score;
        return total / static_cast<float>(recent.size());
    }

    std::optional<ScenarioPerf> UserProfile::getRun(const ScenarioRunId &run_id) const {
        const auto it = m_run_index.find(run_id);
        if (it == m_run_index.end()) return std::nullopt;
        return m_runs[it->second];
    }

    std::optional<std::chrono::sys_seconds> UserProfile::getLastRunTime(const ScenarioId &scenario) const {
        const auto it = m_scenario_index.find(scenario);
        using namespace std::chrono;
        if (it == m_scenario_index.end() || it->second.empty()) return std::nullopt;
        return m_runs[it->second.back()].run_id.startSecond();
    }

    std::optional<double> UserProfile::getTotalTime(const ScenarioId &scenario) const {
        const auto it = m_scenario_aggregate.find(scenario);
        if (it == m_scenario_aggregate.end()) return std::nullopt;
        return it->second.total_time_seconds;
    }

    double UserProfile::getTotalTimeAllScenarios() const {
        return m_total_time_all_scenarios;
    }

    std::optional<std::size_t> UserProfile::getRunCount(const ScenarioId &scenario) const {
        const auto it = m_scenario_aggregate.find(scenario);
        if (it == m_scenario_aggregate.end()) return std::nullopt;
        return it->second.run_count;
    }

    const std::string &UserProfile::getSourceDirectory() const {
        return m_source_directory;
    }

    std::vector<std::pair<std::chrono::sys_days, double> >
    UserProfile::getRollingTimeAverage(const ScenarioId &scenario, const int window_days) const {
        const auto it = m_scenario_index.find(scenario);
        if (it == m_scenario_index.end()) return {};
        return rollingTimeAverageFor(it->second, window_days);
    }

    std::vector<std::pair<std::chrono::sys_days, double> >
    UserProfile::getRollingTimeAverage(const int window_days) const {
        std::vector<std::size_t> all_indices(m_runs.size());
        std::iota(all_indices.begin(), all_indices.end(), 0);
        std::ranges::sort(all_indices, {}, [this](const std::size_t idx) {
            return m_runs[idx].run_id.start_time;
        });
        return rollingTimeAverageFor(all_indices, window_days);
    }

    std::vector<std::pair<std::chrono::sys_days, double> >
    UserProfile::rollingTimeAverageFor(const std::vector<std::size_t> &sorted_indices, const int window_days) const {
        using namespace std::chrono;
        std::vector<std::pair<sys_days, double> > result;
        if (window_days <= 0 || sorted_indices.empty()) return result;

        std::vector<std::pair<sys_days, double> > daily_totals;
        for (const auto idx: sorted_indices) {
            const auto &perf = m_runs[idx];
            // Skip pre-epoch timestamps (malformed/test data) to avoid stretching axis back to 1970
            if (perf.run_id.start_time <= 0) continue;
            const auto day = perf.run_id.startDay();
            if (!daily_totals.empty() && daily_totals.back().first == day) {
                daily_totals.back().second += perf.scenario_length;
            } else {
                daily_totals.emplace_back(day, perf.scenario_length);
            }
        }

        if (daily_totals.empty()) return result;

        const sys_days first_day = daily_totals.front().first;
        const sys_days last_day = daily_totals.back().first;
        const auto total_days = static_cast<std::size_t>((last_day - first_day).count()) + 1;

        // Dense per-day series with implicit 0 for gap days (no play)
        std::vector<double> dense(total_days, 0.0);
        for (const auto &[day, total]: daily_totals) {
            dense[static_cast<std::size_t>((day - first_day).count())] = total;
        }

        const auto window = static_cast<std::size_t>(window_days);
        result.reserve(total_days);
        double window_sum = 0.0;
        for (std::size_t i = 0; i < total_days; ++i) {
            window_sum += dense[i];
            if (i >= window) {
                window_sum -= dense[i - window];
            }
            const std::size_t window_len = std::min(i + 1, window);
            result.emplace_back(first_day + days{static_cast<int>(i)}, window_sum / static_cast<double>(window_len));
        }

        return result;
    }

    const std::vector<ScenarioPerf> &UserProfile::getAllRunRecords() const {
        return m_runs;
    }
}
