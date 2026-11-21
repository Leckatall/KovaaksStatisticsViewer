#include "scenario_evaluation.h"

#include <algorithm>
#include <cstddef>

namespace ksv::domain {
    std::optional<ScenarioEvaluation> ScenarioEvaluation::fromRuns(const std::span<const TimedScore> chronological) {
        if (chronological.empty()) return std::nullopt;

        ScenarioEvaluation evaluation;
        evaluation.m_runCount = static_cast<int>(chronological.size());
        evaluation.m_recentAverages.reserve(chronological.size());
        const TimedScore *best = nullptr;
        for (std::size_t index = 0; index < chronological.size(); ++index) {
            const auto &run = chronological[index];
            if (best == nullptr || run.score > best->score) {
                best = &run;
                evaluation.m_personalBestHistory.push_back(
                    {static_cast<int>(index) + 1, static_cast<double>(run.score)});
            }
            evaluation.m_playtimeSeconds += run.duration_seconds;

            // Re-summed oldest first rather than kept as a running total, so each value is exactly the
            // mean of its own window, with no drift from runs that slid out before it.
            const auto sampleCount = std::min(static_cast<std::size_t>(kRecentRuns), index + 1);
            double total = 0.0;
            for (const auto &sample: chronological.subspan(index + 1 - sampleCount, sampleCount))
                total += sample.score;
            evaluation.m_recentAverages.push_back(total / static_cast<double>(sampleCount));
        }
        evaluation.m_personalBest = evaluation.m_personalBestHistory.back().score;
        evaluation.m_personalBestTime = best->startSecond();
        evaluation.m_recentSampleCount = std::min(kRecentRuns, evaluation.m_runCount);
        evaluation.m_recentAverage = evaluation.m_recentAverages.back();
        return evaluation;
    }

    int ScenarioEvaluation::runCount() const { return m_runCount; }
    double ScenarioEvaluation::personalBest() const { return m_personalBest; }
    std::chrono::sys_seconds ScenarioEvaluation::personalBestTime() const { return m_personalBestTime; }
    double ScenarioEvaluation::recentAverage() const { return m_recentAverage; }
    int ScenarioEvaluation::recentSampleCount() const { return m_recentSampleCount; }
    double ScenarioEvaluation::playtimeSeconds() const { return m_playtimeSeconds; }
    const std::vector<PersonalBestEntry> &ScenarioEvaluation::personalBestHistory() const {
        return m_personalBestHistory;
    }
    const std::vector<double> &ScenarioEvaluation::recentAverages() const { return m_recentAverages; }
}
