#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_EVALUATOR_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_EVALUATOR_H

#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_projection.h"
#include "benchmarks/benchmark_resolution.h"
#include "benchmarks/benchmark_validation.h"
#include "run.h"

namespace ksv::domain {
    [[nodiscard]] std::optional<std::vector<double> > thresholdLadder(
        const std::vector<Tier> &tiers, const std::vector<Threshold> &thresholds);

    [[nodiscard]] double normalizedTierValue(const std::vector<Tier> &tiers,
                                             const std::vector<Threshold> &thresholds,
                                             std::optional<double> score);

    [[nodiscard]] BenchmarkProjection evaluateBenchmark(
        const Benchmark &definition, const CompletenessResult &completeness,
        const std::vector<ScenarioResolution> &resolutions, const std::vector<RunFact> &runFacts,
        std::vector<std::pair<std::chrono::sys_days, double> > rollingPlaytime);
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_EVALUATOR_H
