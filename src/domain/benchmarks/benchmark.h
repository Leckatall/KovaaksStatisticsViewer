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
    struct RankBlocker {
        ScenarioEntryId entryId;
        std::optional<GroupId> group;
        double required = 0.0;
        std::optional<double> current;
    };

    class Benchmark;

    class ScenarioBenchmarkEvaluation {
    public:
        [[nodiscard]] const ScenarioEntryId &entryId() const;
        [[nodiscard]] const std::string &name() const;
        [[nodiscard]] ScenarioMatchState matchState() const;
        [[nodiscard]] const ScenarioEvaluation &stats() const;
        [[nodiscard]] std::optional<TierId> attainedTier() const;
        [[nodiscard]] std::optional<Threshold> nextThreshold() const;
        [[nodiscard]] double normalizedRank() const;

        [[nodiscard]] std::optional<double> thresholdFor(const TierId &tier) const;
        [[nodiscard]] const std::optional<std::vector<double>> &ladder() const;
        [[nodiscard]] bool attains(std::size_t tierIndex) const;
        [[nodiscard]] double normalized(std::optional<double> score) const;

    private:
        friend class Benchmark;
        ScenarioBenchmarkEvaluation() = default;

        // TODO(2026-11-19): placeholder storage, replaced by U3
        ScenarioEntryId m_entryId;
        ScenarioEvaluation m_stats;
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

        // TODO(2026-11-19): placeholder storage, replaced by U3
        GroupId m_id;
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
        [[nodiscard]] const ScenarioBenchmarkEvaluation *scenario(const ScenarioEntryId &id) const;

        [[nodiscard]] std::optional<TierId> attainedRank() const;
        [[nodiscard]] std::optional<TierId> completedRank() const;
        [[nodiscard]] std::optional<TierId> nextTier() const;
        [[nodiscard]] const std::vector<RankBlocker> &nextTierBlockers() const;
        [[nodiscard]] int scenariosAtNextTier() const;
        [[nodiscard]] std::optional<double> averageRank() const;
        [[nodiscard]] const std::vector<AverageRankPoint> &averageRankHistory() const;
        [[nodiscard]] const std::vector<AverageRankPoint> &recentAverageRankHistory() const;

        [[nodiscard]] double totalPlaytimeSeconds() const;
        [[nodiscard]] std::optional<std::chrono::sys_days> latestRunDay() const;
        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double>> rollingPlaytime(int windowDays) const;

    private:
        // TODO(2026-11-19): placeholder storage, replaced by U3
        BenchmarkDefinition m_definition;
        CompletenessResult m_completeness;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_H
