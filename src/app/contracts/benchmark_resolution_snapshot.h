#pragma once

#include <map>
#include <vector>

#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_resolution.h"
#include "run.h"

namespace ksv::application {
    enum class AutomaticMappingWriteError { Conflict, WriteFailed };

    // Derived join of accepted benchmark definitions and profile scenario identity. It carries the
    // identifiers needed to address a result but never a copy of an accepted `domain::Benchmark`:
    // `BenchmarksService` remains the only owner of accepted state.
    struct BenchmarkResolutionSnapshot {
        std::vector<domain::ScenarioId> scenarioCatalogue;
        std::map<domain::BenchmarkId, std::vector<domain::ScenarioResolution>> resolutions;
        // Keyed by the benchmark whose automatic replacement the data service rejected; the entry
        // stays derived as auto-mappable while this records that the write did not land.
        std::map<domain::BenchmarkId, AutomaticMappingWriteError> automaticWriteFailures;

        [[nodiscard]] bool anyAutomaticWriteFailed() const { return !automaticWriteFailures.empty(); }
    };
}
