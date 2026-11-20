#include "scenario_evaluation.h"

#include <algorithm>
#include <cstddef>

namespace ksv::domain {
    ScenarioEvaluation::ScenarioEvaluation(const std::span<const TimedScore> chronological)
        : m_runCount(static_cast<int>(chronological.size())) {
        if (chronological.empty()) return;

        const auto *best = &chronological.front();
        for (const auto &run: chronological) {
            if (run.score > best->score) best = &run;
            m_playtimeSeconds += run.duration_seconds;
        }
        m_personalBest = static_cast<double>(best->score);
        m_personalBestTime = best->startSecond();

        m_recentSampleCount = std::min(kRecentRuns, m_runCount);
        double total = 0.0;
        for (const auto &run: chronological.last(static_cast<std::size_t>(m_recentSampleCount))) total += run.score;
        m_recentAverage = total / static_cast<double>(m_recentSampleCount);
    }

    int ScenarioEvaluation::runCount() const { return m_runCount; }
    std::optional<double> ScenarioEvaluation::personalBest() const { return m_personalBest; }
    std::optional<std::chrono::sys_seconds> ScenarioEvaluation::personalBestTime() const { return m_personalBestTime; }
    std::optional<double> ScenarioEvaluation::recentAverage() const { return m_recentAverage; }
    int ScenarioEvaluation::recentSampleCount() const { return m_recentSampleCount; }
    double ScenarioEvaluation::playtimeSeconds() const { return m_playtimeSeconds; }
}
