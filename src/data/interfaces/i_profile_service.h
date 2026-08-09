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
        // Loads from cache if present, otherwise falls back to generateProfileFromDirectory()
        virtual void loadProfile() = 0;

        [[nodiscard]] virtual std::vector<domain::ScenarioId> getScenarioList() const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getPerf(const std::string& path) const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getLatestPerf() const = 0;

        [[nodiscard]] virtual std::optional<domain::ScenarioPerf> getMostRecentPerf(
            const domain::ScenarioId& scenario) const = 0;

        [[nodiscard]] virtual std::optional<float> getAverageScore(
            const domain::ScenarioId& scenario, std::size_t count) const = 0;

        [[nodiscard]] virtual std::vector<std::pair<std::chrono::sys_days, double>>
        getRollingTimeAverage(int window_days) const = 0;

        [[nodiscard]] virtual bool isProfileLoaded() const = 0;

        virtual void onProfileChanged(std::function<void()> callback) = 0;
    };
}
#endif
