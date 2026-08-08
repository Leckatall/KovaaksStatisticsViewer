//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PROFILE_SEVICE_H
#define KOVAAKSSTATSVIEWER_I_PROFILE_SEVICE_H
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include "domain/scenario_perf.h"

namespace ksv::application {
    class IProfileService {
    public:
        virtual ~IProfileService() = default;

        virtual void generateProfileFromDirectory() = 0;

        // Loads the profile from its on-disk cache if present, otherwise falls
        // back to generateProfileFromDirectory() to build and cache it fresh.
        virtual void loadProfile() = 0;

        [[nodiscard]] virtual std::vector<domain::ScenarioId> getScenarioList() const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getPerf(const std::string& path) const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getLatestPerf() const = 0;

        // Most recent run for a specific scenario, or nullopt if the profile
        // hasn't been generated yet or has no runs for that scenario.
        [[nodiscard]] virtual std::optional<domain::ScenarioPerf> getMostRecentPerf(
            const domain::ScenarioId& scenario) const = 0;

        // Average final score across the `count` most recent runs for a scenario.
        [[nodiscard]] virtual std::optional<float> getAverageScore(
            const domain::ScenarioId& scenario, std::size_t count) const = 0;

        // Per-day rolling average of daily playtime (summed scenario_length,
        // seconds) over a trailing window of `window_days`, across all
        // scenarios. One (day, average-seconds) pair per calendar day from the
        // first to the last day with runs; gap days count as 0. Empty if no
        // profile is loaded or it has no runs.
        [[nodiscard]] virtual std::vector<std::pair<std::chrono::sys_days, double>>
        getRollingTimeAverage(int window_days) const = 0;

        // True once a profile (loaded from cache or freshly generated) is held in memory.
        [[nodiscard]] virtual bool isProfileLoaded() const = 0;

        virtual void onProfileChanged(std::function<void()> callback) = 0;
    };
}
#endif
