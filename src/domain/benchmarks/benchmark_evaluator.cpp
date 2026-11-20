#include "benchmarks/benchmark_evaluator.h"

namespace ksv::domain {
    namespace {
        ScenarioProjection projectScenario(const ScenarioBenchmarkEvaluation &scenario) {
            const auto &stats = scenario.stats();
            return {
                .entryId = scenario.entryId(),
                .name = scenario.name(),
                .matchState = scenario.matchState(),
                .personalBest = stats.personalBest(),
                .personalBestTime = stats.personalBestTime(),
                .recentAverage = stats.recentAverage(),
                .recentSampleCount = stats.recentSampleCount(),
                .runCount = stats.runCount(),
                .attainedTier = scenario.attainedTier(),
                .nextThreshold = scenario.nextThreshold(),
                .normalizedRank = scenario.normalizedRank(),
                .playtimeSeconds = stats.playtimeSeconds(),
            };
        }

        GroupProjection projectGroup(const GroupEvaluation &group) {
            GroupProjection projection{group.id(), group.name(), group.satisfiedTier(), {}, {}};
            for (const auto *scenario: group.scenarios()) projection.scenarios.push_back(scenario->entryId());
            for (const auto *subgroup: group.subgroups()) projection.subgroups.push_back(projectGroup(*subgroup));
            return projection;
        }
    }

    // TODO(2026-11-19): Remove with Cleanup when the app and UI consume Benchmark directly.
    BenchmarkProjection toProjection(const Benchmark &benchmark,
                                     std::vector<std::pair<std::chrono::sys_days, double>> rollingPlaytime) {
        BenchmarkProjection projection{
            .benchmarkId = benchmark.id(),
            .completeness = benchmark.completeness().completeness,
            .tiers = benchmark.tiers(),
            .scenarios = {},
            .uncategorized = {},
            .categories = {},
            .attainedRank = benchmark.attainedRank(),
            .completedRank = benchmark.completedRank(),
            .nextTier = benchmark.nextTier(),
            .nextTierBlockers = benchmark.nextTierBlockers(),
            .scenariosAtNextTier = benchmark.scenariosAtNextTier(),
            .averageRank = benchmark.averageRank(),
            .averageRankHistory = benchmark.averageRankHistory(),
            .recentAverageRankHistory = benchmark.recentAverageRankHistory(),
            .totalPlaytimeSeconds = benchmark.totalPlaytimeSeconds(),
            .rollingPlaytime = std::move(rollingPlaytime),
        };
        for (const auto *scenario: benchmark.scenarios()) projection.scenarios.push_back(projectScenario(*scenario));
        for (const auto *scenario: benchmark.uncategorized()) projection.uncategorized.push_back(scenario->entryId());
        for (const auto *category: benchmark.categories()) projection.categories.push_back(projectGroup(*category));
        return projection;
    }

    // TODO(2026-11-19): Remove with Cleanup when the app and UI consume Benchmark directly.
    BenchmarkProjection evaluateBenchmark(
        const BenchmarkDefinition &definition, const CompletenessResult &completeness,
        const std::vector<ScenarioResolution> &resolutions, const std::vector<RunFact> &runFacts,
        std::vector<std::pair<std::chrono::sys_days, double>> rollingPlaytime) {
        return toProjection(Benchmark(definition, completeness, resolutions, runFacts), std::move(rollingPlaytime));
    }
}
