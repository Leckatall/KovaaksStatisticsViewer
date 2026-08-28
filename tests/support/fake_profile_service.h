#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_PROFILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_PROFILE_SERVICE_H

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "data/interfaces/i_profile_service.h"

namespace ksv::tests_support {
    class FakeProfileService final : public application::IProfileService {
    public:
        int generate_call_count = 0;
        int begin_build_count = 0;
        int apply_count = 0;
        bool profile_loaded = false;
        std::vector<domain::ScenarioId> scenario_list;
        std::vector<domain::ScenarioId> scenarios;
        std::vector<domain::Run> runs;
        std::unordered_map<std::string, domain::Run> perf_by_path;
        std::unordered_map<domain::ScenarioRunId, domain::Run> run_by_id;
        domain::Run latest_run;
        std::unordered_map<std::string, std::vector<domain::Run>> most_recent_perfs_by_hash;
        std::unordered_map<domain::ScenarioId, std::vector<domain::Run>> perfs_by_scenario;
        std::unordered_map<domain::ScenarioId, std::vector<domain::RunSummary>> completion_history_by_scenario;
        std::vector<domain::RunSummary> completion_history;
        std::unordered_map<domain::ScenarioId, std::size_t> run_counts;
        std::unordered_map<domain::ScenarioId, double> total_times;
        std::vector<domain::Run> recent_runs;
        mutable int completion_history_calls = 0;
        mutable domain::ScenarioId requested_scenario;
        std::function<void()> stored_callback;
        std::function<void()> stored_build_requester;
        std::optional<domain::UserProfile> applied_profile;

        void generateProfileFromDirectory() override { ++generate_call_count; }
        void loadProfile() override { ++generate_call_count; }
        void onBuildRequested(std::function<void()> callback) override { stored_build_requester = std::move(callback); }
        void beginProfileBuild() override { ++begin_build_count; }

        void applyBuiltProfile(domain::UserProfile profile) override {
            ++apply_count;
            applied_profile = std::move(profile);
            scenario_list = applied_profile->getScenarioList();
            profile_loaded = true;
            notifyProfileChanged();
        }

        [[nodiscard]] std::vector<domain::ScenarioId> getScenarioList() const override {
            return scenarios.empty() ? scenario_list : scenarios;
        }

        [[nodiscard]] domain::Run getPerf(const std::string &path) const override { return perf_by_path.at(path); }
        [[nodiscard]] domain::Run getLatestRun() const override { return latest_run; }

        [[nodiscard]] std::optional<domain::Run> getMostRecentRun(const domain::ScenarioId &scenario) const override {
            const auto perfs = getMostRecentRuns(scenario, 1);
            return perfs.empty() ? std::nullopt : std::optional{perfs.back()};
        }

        [[nodiscard]] std::vector<domain::Run> getMostRecentRuns(
            const domain::ScenarioId &scenario, const std::size_t count) const override {
            const auto by_hash = most_recent_perfs_by_hash.find(scenario.hash);
            const auto by_scenario = perfs_by_scenario.find(scenario);
            const auto &source = by_hash != most_recent_perfs_by_hash.end() ? by_hash->second
                               : by_scenario != perfs_by_scenario.end() ? by_scenario->second
                               : empty_perfs;
            const auto n = std::min(count, source.size());
            return {source.end() - static_cast<std::ptrdiff_t>(n), source.end()};
        }

        [[nodiscard]] std::vector<domain::Run> getRunsForScenario(
            const domain::ScenarioId &scenario) const override {
            if (const auto it = perfs_by_scenario.find(scenario); it != perfs_by_scenario.end()) return it->second;
            if (const auto it = most_recent_perfs_by_hash.find(scenario.hash); it != most_recent_perfs_by_hash.end()) return it->second;
            std::vector<domain::Run> result;
            for (const auto &run: runs) if (run.run_id.scenario_id == scenario) result.push_back(run);
            return result;
        }

        [[nodiscard]] std::vector<domain::RunSummary> getCompletionHistory(
            const domain::ScenarioId &scenario) const override {
            ++completion_history_calls;
            requested_scenario = scenario;
            if (const auto it = completion_history_by_scenario.find(scenario); it != completion_history_by_scenario.end()) {
                return it->second;
            }
            return completion_history;
        }

        [[nodiscard]] std::optional<float> getAverageScore(const domain::ScenarioId &, std::size_t) const override {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<domain::Run> getCurrentRun(const domain::ScenarioRunId &run_id) const override {
            const auto it = run_by_id.find(run_id);
            return it == run_by_id.end() ? std::nullopt : std::optional{it->second};
        }

        [[nodiscard]] std::optional<std::size_t> getRunCount(const domain::ScenarioId &scenario) const override {
            const auto it = run_counts.find(scenario);
            return it == run_counts.end() ? std::nullopt : std::optional{it->second};
        }

        [[nodiscard]] std::optional<std::chrono::sys_seconds> getLastRunTime(const domain::ScenarioId &) const override {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<double> getTotalTime(const domain::ScenarioId &scenario) const override {
            const auto it = total_times.find(scenario);
            return it == total_times.end() ? std::nullopt : std::optional{it->second};
        }

        [[nodiscard]] std::vector<domain::Run> getRecentRuns(const std::size_t count) const override {
            auto result = recent_runs;
            if (result.size() > count) result.resize(count);
            return result;
        }

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double>> getRollingTimeAverage(int) const override {
            return {};
        }

        [[nodiscard]] bool isProfileLoaded() const override { return profile_loaded; }
        void onProfileChanged(std::function<void()> callback) override { stored_callback = std::move(callback); }
        void notifyProfileChanged() const { if (stored_callback) stored_callback(); }

    private:
        inline static const std::vector<domain::Run> empty_perfs;
    };
}

#endif
