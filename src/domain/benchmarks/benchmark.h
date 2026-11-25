#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_H

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "benchmarks/benchmark_definition.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_resolution.h"
#include "benchmarks/benchmark_validation.h"
#include "benchmarks/rank_history.h"
#include "run.h"
#include "scenario_evaluation.h"

namespace ksv::domain {
    struct RankBlockerScenario {
        BenchmarkScenarioId entryId;
        double required = 0.0;
        std::optional<double> current;
    };

    // One unmet rank requirement. Any one of a group's scenarios attaining the tier would meet it.
    struct RankBlocker {
        /// Empty for an Uncategorized scenario, which is a requirement on its own.
        std::optional<GroupId> group;
        /// In definition order; exactly one without a group.
        std::vector<RankBlockerScenario> scenarios;
    };

    struct NormalizedPersonalBest {
        /// As PersonalBestEntry::scenarioRun.
        int scenarioRun = 0;
        double normalizedRank = 0.0;
    };

    class Benchmark;

    class ScenarioBenchmarkEvaluation {
    public:
        [[nodiscard]] const BenchmarkScenarioId &entryId() const;
        [[nodiscard]] const std::string &name() const;
        [[nodiscard]] ScenarioMatchState matchState() const;
        /** @return Nothing when the entry has no runs. */
        [[nodiscard]] const std::optional<ScenarioEvaluation> &stats() const;
        [[nodiscard]] std::optional<TierId> attainedTier() const;
        [[nodiscard]] std::optional<Threshold> nextThreshold() const;
        [[nodiscard]] double normalizedRank() const;
        [[nodiscard]] double normalizedRecentRank() const;
        /** @return The stats' series on the ladder; empty without runs. */
        [[nodiscard]] std::vector<NormalizedPersonalBest> normalizedPersonalBestHistory() const;
        [[nodiscard]] std::vector<double> normalizedRecentAverages() const;

        [[nodiscard]] std::optional<double> thresholdFor(const TierId &tier) const;
        [[nodiscard]] const std::optional<std::vector<double>> &ladder() const;
        [[nodiscard]] bool attains(std::size_t tierIndex) const;
        [[nodiscard]] double normalized(std::optional<double> score) const;

    private:
        friend class Benchmark;
        ScenarioBenchmarkEvaluation() = default;

        BenchmarkScenario m_entry;
        ScenarioMatchState m_matchState = ScenarioMatchState::Unresolved;
        std::optional<ScenarioEvaluation> m_stats;
        std::optional<std::vector<double>> m_ladder;
        std::optional<TierId> m_attainedTier;
        std::optional<Threshold> m_nextThreshold;
        double m_normalizedRank = 0.0;
        double m_normalizedRecentRank = 0.0;
    };

    class GroupEvaluation {
    public:
        [[nodiscard]] const GroupId &id() const;
        [[nodiscard]] const std::string &name() const;
        [[nodiscard]] BenchmarkColor color() const;
        [[nodiscard]] std::optional<TierId> satisfiedTier() const;
        [[nodiscard]] std::vector<const ScenarioBenchmarkEvaluation *> scenarios() const;
        [[nodiscard]] std::vector<const GroupEvaluation *> subgroups() const;

    private:
        friend class Benchmark;
        GroupEvaluation() = default;

        GroupId m_id;
        std::string m_name;
        BenchmarkColor m_color;
        std::optional<TierId> m_satisfiedTier;
        std::vector<ScenarioBenchmarkEvaluation> m_scenarios;
        std::vector<GroupEvaluation> m_subgroups;
    };

    class Benchmark {
    public:
        // Run facts must be chronological. Hashes outside the definition are ignored.
        Benchmark(BenchmarkDefinition definition, CompletenessResult completeness,
                  const std::vector<ScenarioResolution> &resolutions, std::span<const RunFact> runFacts);

        [[nodiscard]] const BenchmarkDefinition &definition() const;
        [[nodiscard]] const BenchmarkId &id() const;
        [[nodiscard]] const CompletenessResult &completeness() const;
        [[nodiscard]] const std::vector<Tier> &tiers() const;

        // Pointees belong to this benchmark and are valid until it is destroyed, assigned to or moved from.
        [[nodiscard]] std::vector<const ScenarioBenchmarkEvaluation *> uncategorized() const;
        [[nodiscard]] std::vector<const GroupEvaluation *> categories() const;
        [[nodiscard]] std::vector<const ScenarioBenchmarkEvaluation *> scenarios() const;
        [[nodiscard]] const ScenarioBenchmarkEvaluation *scenario(const BenchmarkScenarioId &id) const;

        [[nodiscard]] std::optional<TierId> attainedRank() const;
        [[nodiscard]] std::optional<TierId> completedRank() const;
        [[nodiscard]] std::optional<TierId> nextTier() const;
        [[nodiscard]] const std::vector<RankBlocker> &nextTierBlockers() const;
        [[nodiscard]] int scenariosAtNextTier() const;
        [[nodiscard]] std::optional<double> averageRank() const;
        /// Where recentAverageRankHistory() ends; none without a member run.
        [[nodiscard]] std::optional<double> recentAverageRank() const;
        [[nodiscard]] const std::vector<AverageRankAfterRun> &averageRankHistory() const;
        [[nodiscard]] const std::vector<AverageRankAfterRun> &recentAverageRankHistory() const;
        /// The start of benchmark run `n` at index `n - 1`; filled only when the histories are.
        [[nodiscard]] const std::vector<std::chrono::sys_seconds> &memberRunStarts() const;

        [[nodiscard]] double totalPlaytimeSeconds() const;
        [[nodiscard]] std::optional<std::chrono::sys_days> latestRunDay() const;
        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double>> rollingPlaytime(int windowDays) const;

    private:
        struct Builder;

        BenchmarkDefinition m_definition;
        CompletenessResult m_completeness;
        std::vector<ScenarioBenchmarkEvaluation> m_uncategorized;
        std::vector<GroupEvaluation> m_categories;
        std::optional<TierId> m_attainedRank;
        std::optional<TierId> m_completedRank;
        std::optional<TierId> m_nextTier;
        std::vector<RankBlocker> m_nextTierBlockers;
        int m_scenariosAtNextTier = 0;
        std::optional<double> m_averageRank;
        std::vector<AverageRankAfterRun> m_averageRankHistory;
        std::vector<AverageRankAfterRun> m_recentAverageRankHistory;
        std::vector<std::chrono::sys_seconds> m_memberRunStarts;
        double m_totalPlaytimeSeconds = 0.0;
        std::optional<std::chrono::sys_days> m_latestRunDay;
        std::vector<TimedScore> m_memberScores;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_H
