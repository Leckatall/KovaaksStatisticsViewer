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
    UserProfile::UserProfile(SourceRegistry sources) : m_sources(std::move(sources)) {
    }

    bool UserProfile::addRun(const Run &run) {
        if (m_run_index.contains(run.run_id)) {
            std::cerr << "Duplicate run_id for scenario '" << run.run_id.scenario_id.name
                    << "' at start_time " << run.run_id.start_time << ", skipping." << std::endl;
            return false;
        }

        const std::size_t index = m_runs.size();
        m_runs.push_back(run);
        m_run_index.emplace(run.run_id, index);

        const ScenarioId &scenario = run.run_id.scenario_id;
        auto &indices = m_scenario_index[scenario];
        const auto insert_pos = std::ranges::upper_bound(indices, run.run_id.start_time, {},
                                                         [this](const std::size_t idx) {
                                                             return m_runs[idx].run_id.start_time;
                                                         });
        indices.insert(insert_pos, index);

        auto &aggregate = m_scenario_aggregate[scenario];
        aggregate.total_time_seconds += run.scenario_length;
        aggregate.run_count += 1;
        m_total_time_all_scenarios += run.scenario_length;

        if (!m_most_recent_index || run.run_id.start_time > m_runs[*m_most_recent_index].run_id.start_time) {
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

    std::optional<Run> UserProfile::getLatestRun() const {
        if (!m_most_recent_index) return std::nullopt;
        return m_runs[*m_most_recent_index];
    }

    std::optional<Run> UserProfile::getMostRecentRun(const ScenarioId &scenario) const {
        const auto it = m_scenario_index.find(scenario);
        if (it == m_scenario_index.end() || it->second.empty()) return std::nullopt;
        return m_runs[it->second.back()];
    }

    std::vector<Run>
    UserProfile::getMostRecentRuns(const ScenarioId &scenario, const std::size_t count) const {
        const auto it = m_scenario_index.find(scenario);
        if (it == m_scenario_index.end() || count == 0) return {};
        const auto &indices = it->second;
        const auto n = std::min(count, indices.size());

        std::vector<Run> result;
        result.reserve(n);
        for (auto idx_it = indices.end() - static_cast<std::ptrdiff_t>(n); idx_it != indices.end(); ++idx_it) {
            result.push_back(m_runs[*idx_it]);
        }
        return result;
    }

    std::vector<Run> UserProfile::getRunsForScenario(const ScenarioId &scenario) const {
        const auto it = m_scenario_index.find(scenario);
        if (it == m_scenario_index.end()) return {};
        std::vector<Run> result;
        result.reserve(it->second.size());
        for (const auto index: it->second) result.push_back(m_runs[index]);
        return result;
    }

    std::vector<RunSummary> UserProfile::getCompletionHistory(const ScenarioId &scenario) const {
        const auto it = m_scenario_index.find(scenario);
        if (it == m_scenario_index.end()) return {};

        std::vector<RunSummary> result;
        result.reserve(it->second.size());
        for (const std::size_t idx: it->second) {
            const auto &run = m_runs[idx];
            result.push_back({run.run_id, run.totals()});
        }
        return result;
    }

    std::optional<float> UserProfile::getAverageScore(const ScenarioId &scenario, const std::size_t count) const {
        const auto recent = getMostRecentRuns(scenario, count);
        if (recent.empty()) return std::nullopt;

        double total = 0.0;
        for (const auto &run: recent) total += run.totals().score;
        return static_cast<float>(total / static_cast<double>(recent.size()));
    }

    std::optional<Run> UserProfile::getCurrentRun(const ScenarioRunId &run_id) const {
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

    const SourceRegistry &UserProfile::sources() const {
        return m_sources;
    }

    DirectoryId UserProfile::ensureSource(const std::string &root, const std::string &subdir) {
        const auto root_id = m_sources.ensure({}, root);
        return m_sources.ensure(root_id, subdir);
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
            const auto &run = m_runs[idx];
            // Skip pre-epoch timestamps (malformed/test data) to avoid stretching axis back to 1970
            if (run.run_id.start_time <= 0) continue;
            const auto day = run.run_id.startDay();
            if (!daily_totals.empty() && daily_totals.back().first == day) {
                daily_totals.back().second += run.scenario_length;
            } else {
                daily_totals.emplace_back(day, run.scenario_length);
            }
        }

        if (daily_totals.empty()) return result;

        const sys_days first_day = daily_totals.front().first;
        const sys_days last_day = daily_totals.back().first;
        const auto window = days{window_days};

        result.reserve(daily_totals.size() * static_cast<std::size_t>(window_days));
        std::size_t lo = 0;
        std::size_t next = 0;
        double window_sum = 0.0;
        for (sys_days day = first_day; day <= last_day;) {
            if (next < daily_totals.size() && daily_totals[next].first == day) {
                window_sum += daily_totals[next].second;
                ++next;
            }
            while (lo < next && daily_totals[lo].first <= day - window) {
                window_sum -= daily_totals[lo].second;
                ++lo;
            }
            if (lo == next) {
                window_sum = 0.0;
            }
            if (window_sum != 0.0) {
                const auto window_len = std::min(static_cast<std::size_t>((day - first_day).count()) + 1,
                                                 static_cast<std::size_t>(window_days));
                result.emplace_back(day, window_sum / static_cast<double>(window_len));
                day += days{1};
            } else {
                day = (next < daily_totals.size()) ? daily_totals[next].first : last_day + days{1};
            }
        }

        return result;
    }

    const std::vector<Run> &UserProfile::getAllRunRecords() const {
        return m_runs;
    }
}
