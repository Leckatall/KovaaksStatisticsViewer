#ifndef KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H
#define KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H

#include <chrono>
#include <optional>
#include <span>
#include <vector>

#include "run.h"

namespace ksv::domain {
    struct PersonalBestEntry {
        /// 1-based position in the run sequence the evaluation was built from.
        int scenarioRun = 0;
        double score = 0.0;

        bool operator==(const PersonalBestEntry &) const = default;
    };

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
        /// The first run and every later run that strictly beat the PB before it.
        [[nodiscard]] const std::vector<PersonalBestEntry> &personalBestHistory() const;
        /// Entry `i` is the recent average after scenario run `i + 1`.
        [[nodiscard]] const std::vector<double> &recentAverages() const;

        bool operator==(const ScenarioEvaluation &) const = default;

    private:
        ScenarioEvaluation() = default;

        int m_runCount = 0;
        double m_personalBest = 0.0;
        std::chrono::sys_seconds m_personalBestTime{};
        double m_recentAverage = 0.0;
        int m_recentSampleCount = 0;
        double m_playtimeSeconds = 0.0;
        std::vector<PersonalBestEntry> m_personalBestHistory;
        std::vector<double> m_recentAverages;
    };
}

#endif // KOVAAKSSTATSVIEWER_SCENARIO_EVALUATION_H
