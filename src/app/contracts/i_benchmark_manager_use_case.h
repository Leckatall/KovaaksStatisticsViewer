#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "contracts/benchmark_editor_seed.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_resolution.h"
#include "data/interfaces/i_benchmarks_service.h"

namespace ksv::application {
    struct BenchmarkManagerState;

    // Sole application boundary for the Benchmark Manager. It composes accepted-library state and
    // derived resolution state for presentation and accepts whole benchmarks; it retains no working
    // copy and exposes no individual editor mutation. Presentation owns the working copy and its
    // local edits (Task P4); pure structural editing lives in domain::BenchmarkEditor (Task P1).
    class IBenchmarkManagerUseCase {
    public:
        virtual ~IBenchmarkManagerUseCase() = default;

        // Valid until the next service callback; never retained by callers.
        [[nodiscard]] virtual const BenchmarkManagerState &state() const = 0;
        virtual void onChanged(std::function<void()> callback) = 0;

        virtual void refresh() = 0;
        [[nodiscard]] virtual std::string managedDirectoryPath() const = 0;

        // A fresh benchmark seed with a reserved id and no token; the editor is now leased.
        [[nodiscard]] virtual BenchmarkEditorSeed beginNewBenchmark() = 0;
        // A copy of the accepted benchmark plus its admitted token, or std::nullopt for an unknown
        // id or a problem entry that carries no trustworthy token.
        [[nodiscard]] virtual std::optional<BenchmarkEditorSeed> openBenchmark(
            const domain::BenchmarkId &id) = 0;
        virtual void closeEditor() = 0;

        [[nodiscard]] virtual PlaylistSeedImport importPlaylistSeed(const std::string &path) = 0;

        // Submits a complete working value plus its optional admitted token; the data service's
        // classified outcome is returned unchanged.
        [[nodiscard]] virtual data::BenchmarkSaveOutcome save(
            const domain::Benchmark &benchmark,
            const std::optional<data::BenchmarkEditToken> &token) = 0;
        // Resolves a caller-supplied working copy against the latest profile catalogue without
        // adopting or persisting it.
        [[nodiscard]] virtual std::vector<domain::ScenarioResolution> resolve(
            const domain::Benchmark &benchmark) const = 0;
        [[nodiscard]] virtual data::BenchmarkRemoveOutcome deleteBenchmark(
            const data::BenchmarkEditToken &token) = 0;
    };
}
