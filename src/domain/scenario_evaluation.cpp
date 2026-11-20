#include "scenario_evaluation.h"

#include <algorithm>
#include <cstddef>

namespace ksv::domain {
    std::optional<ScenarioEvaluation> ScenarioEvaluation::fromRuns(const std::span<const TimedScore> chronological) {
        if (chronological.empty()) return std::nullopt;

        ScenarioEvaluation evaluation;
        evaluation.m_runCount = static_cast<int>(chronological.size());
        const auto *best = &chronological.front();
        for (const auto &run: chronological) {
            if (run.score > best->score) best = &run;
            evaluation.m_playtimeSeconds += run.duration_seconds;
        }
        evaluation.m_personalBest = static_cast<double>(best->score);
        evaluation.m_personalBestTime = best->startSecond();

        evaluation.m_recentSampleCount = std::min(kRecentRuns, evaluation.m_runCount);
        double total = 0.0;
        for (const auto &run: chronological.last(static_cast<std::size_t>(evaluation.m_recentSampleCount)))
            total += run.score;
        evaluation.m_recentAverage = total / static_cast<double>(evaluation.m_recentSampleCount);
        return evaluation;
    }

    int ScenarioEvaluation::runCount() const { return m_runCount; }
    double ScenarioEvaluation::personalBest() const { return m_personalBest; }
    std::chrono::sys_seconds ScenarioEvaluation::personalBestTime() const { return m_personalBestTime; }
    double ScenarioEvaluation::recentAverage() const { return m_recentAverage; }
    int ScenarioEvaluation::recentSampleCount() const { return m_recentSampleCount; }
    double ScenarioEvaluation::playtimeSeconds() const { return m_playtimeSeconds; }
}
