#include "scenario_evaluation.h"

#include <algorithm>

namespace ksv::domain {
    ScenarioEvaluation::ScenarioEvaluation(const std::span<const TimedScore> chronological)
        : m_runCount(static_cast<int>(chronological.size())) {
        if (chronological.empty()) return;

        const auto *best = &chronological.front();
        for (const auto &run: chronological) {
            if (run.score > best->score || (run.score == best->score && run.start_time < best->start_time))
                best = &run;
            m_playtimeSeconds += run.duration_seconds;
        }
        m_personalBest = static_cast<double>(best->score);
        m_personalBestTime = best->startSecond();

        const auto sampleCount = std::min(kRecentRuns, chronological.size());
        double total = 0.0;
        for (const auto &run: chronological.last(sampleCount)) total += run.score;
        m_recentAverage = total / static_cast<double>(sampleCount);
        m_recentSampleCount = static_cast<int>(sampleCount);
    }

    int ScenarioEvaluation::runCount() const { return m_runCount; }
    std::optional<double> ScenarioEvaluation::personalBest() const { return m_personalBest; }
    std::optional<std::chrono::sys_seconds> ScenarioEvaluation::personalBestTime() const { return m_personalBestTime; }
    std::optional<double> ScenarioEvaluation::recentAverage() const { return m_recentAverage; }
    int ScenarioEvaluation::recentSampleCount() const { return m_recentSampleCount; }
    double ScenarioEvaluation::playtimeSeconds() const { return m_playtimeSeconds; }
}
