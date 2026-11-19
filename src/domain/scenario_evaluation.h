#ifndef KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H
#define KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>

#include "run.h"

namespace ksv::domain {
    class ScenarioEvaluation {
    public:
        static constexpr std::size_t kRecentRuns = 5;

        ScenarioEvaluation() = default;
        // The recent window uses the input order, which must be oldest first.
        explicit ScenarioEvaluation(std::span<const TimedScore> chronological);

        [[nodiscard]] int runCount() const;
        [[nodiscard]] std::optional<double> personalBest() const;
        [[nodiscard]] std::optional<std::chrono::sys_seconds> personalBestTime() const;
        [[nodiscard]] std::optional<double> recentAverage() const;
        [[nodiscard]] int recentSampleCount() const;
        [[nodiscard]] double playtimeSeconds() const;

        bool operator==(const ScenarioEvaluation &) const = default;

    private:
        int m_runCount = 0;
        std::optional<double> m_personalBest;
        std::optional<std::chrono::sys_seconds> m_personalBestTime;
        std::optional<double> m_recentAverage;
        int m_recentSampleCount = 0;
        double m_playtimeSeconds = 0.0;
    };
}

#endif // KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H
