#ifndef KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H
#define KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H

#include <chrono>
#include <optional>
#include <span>

#include "run.h"

namespace ksv::domain {
    class ScenarioEvaluation {
    public:
        static constexpr int kRecentRuns = 5;

        /**
         * @param chronological Oldest first; ties on score resolve to the earliest run and the recent window is the tail.
         * @return Nothing when there are no runs.
         */
        [[nodiscard]] static std::optional<ScenarioEvaluation> fromRuns(std::span<const TimedScore> chronological);

        [[nodiscard]] int runCount() const;
        [[nodiscard]] double personalBest() const;
        [[nodiscard]] std::chrono::sys_seconds personalBestTime() const;
        [[nodiscard]] double recentAverage() const;
        [[nodiscard]] int recentSampleCount() const;
        [[nodiscard]] double playtimeSeconds() const;

        bool operator==(const ScenarioEvaluation &) const = default;

    private:
        ScenarioEvaluation() = default;

        int m_runCount = 0;
        double m_personalBest = 0.0;
        std::chrono::sys_seconds m_personalBestTime{};
        double m_recentAverage = 0.0;
        int m_recentSampleCount = 0;
        double m_playtimeSeconds = 0.0;
    };
}

#endif // KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H
