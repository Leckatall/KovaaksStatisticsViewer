#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "benchmark_resolution_snapshot.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_resolution.h"

namespace ksv::application {
    // Sole owner of the join between accepted benchmark definitions and profile scenario identity.
    // It subscribes to the benchmark and profile change streams and republishes a derived
    // `BenchmarkResolutionSnapshot`; there is no public reconcile() because a reconciliation pass is
    // always a consequence of one of those subscriptions, never a caller action.
    class IBenchmarkResolutionUseCase {
    public:
        virtual ~IBenchmarkResolutionUseCase() = default;

        [[nodiscard]] virtual BenchmarkResolutionSnapshot snapshot() const = 0;

        // Resolves a supplied working copy against the latest profile catalogue without adopting,
        // persisting, or otherwise retaining it.
        [[nodiscard]] virtual std::vector<domain::ScenarioResolution> resolve(
            const domain::Benchmark &benchmark) const = 0;

        // A leased benchmark keeps being resolved for presentation but is excluded from automatic
        // persistence until the lease is cleared with `std::nullopt`.
        virtual void setEditLease(std::optional<domain::BenchmarkId> benchmark) = 0;

        virtual void onChanged(std::function<void()> callback) = 0;
    };
}
