//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PROFILE_SEVICE_H
#define KOVAAKSSTATSVIEWER_I_PROFILE_SEVICE_H
#include <chrono>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "domain/scenario_perf.h"
#include "domain/user_profile.h"

namespace ksv::application {
    class IProfileService {
    public:
        virtual ~IProfileService() = default;

        // Builds synchronously. Prefer the build-request hook below: with a requester
        // installed, a missing or rejected stored profile goes through it instead of blocking here.
        virtual void generateProfileFromDirectory() = 0;

        // Loads the stored profile if present, otherwise falls back to a build.
        virtual void loadProfile() = 0;

        // Installed by whoever owns a build worker. With no requester installed every
        // build path stays synchronous.
        virtual void onBuildRequested(std::function<void()> callback) = 0;

        // Between beginProfileBuild() and applyBuiltProfile(), perf files arriving from
        // the watcher are queued rather than applied, then replayed onto the built
        // profile — a build that scanned the directory before the file landed must not
        // silently drop it.
        virtual void beginProfileBuild() = 0;

        virtual void applyBuiltProfile(domain::UserProfile profile) = 0;

        [[nodiscard]] virtual std::vector<domain::ScenarioId> getScenarioList() const = 0;

        [[nodiscard]] virtual domain::ScenarioPerf getPerf(const std::string &path) const = 0;

        [[nodiscard]] virtual domain::ScenarioPerf getLatestPerf() const = 0;

        [[nodiscard]] virtual std::optional<domain::ScenarioPerf> getMostRecentPerf(
            const domain::ScenarioId &scenario) const = 0;

        [[nodiscard]] virtual std::vector<domain::ScenarioPerf> getMostRecentPerfs(
            const domain::ScenarioId &scenario, std::size_t count) const = 0;

        [[nodiscard]] virtual std::vector<domain::ScenarioPerf> getRunsForScenario(
            const domain::ScenarioId &scenario) const = 0;

        [[nodiscard]] virtual std::vector<domain::RunData>
        getCompletionHistory(const domain::ScenarioId &scenario) const = 0;

        [[nodiscard]] virtual std::optional<float> getAverageScore(
            const domain::ScenarioId &scenario, std::size_t count) const = 0;

        [[nodiscard]] virtual std::optional<domain::ScenarioPerf> getRun(
            const domain::ScenarioRunId &run_id) const = 0;

        [[nodiscard]] virtual std::optional<std::size_t> getRunCount(
            const domain::ScenarioId &scenario) const = 0;

        [[nodiscard]] virtual std::optional<std::chrono::sys_seconds> getLastRunTime(
            const domain::ScenarioId &scenario) const = 0;

        [[nodiscard]] virtual std::optional<double> getTotalTime(
            const domain::ScenarioId &scenario) const = 0;

        // Newest-first, capped at count.
        [[nodiscard]] virtual std::vector<domain::ScenarioPerf> getRecentRuns(std::size_t count) const = 0;

        [[nodiscard]] virtual std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(int window_days) const = 0;

        [[nodiscard]] virtual bool isProfileLoaded() const = 0;

        virtual void onProfileChanged(std::function<void()> callback) = 0;
    };
}
#endif
