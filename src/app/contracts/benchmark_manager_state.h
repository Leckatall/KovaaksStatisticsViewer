#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "benchmark_library_snapshot.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_resolution.h"
#include "benchmarks/benchmark_validation.h"
#include "run.h"

namespace ksv::application {
    // One coherent revision of the Benchmark Manager's application state: the accepted library
    // listing, the active draft, and every derived projection all belong to the same
    // `libraryRevision`. Consumers read it once after an onChanged notification and never retain
    // the reference.
    struct BenchmarkManagerState {
        std::optional<BenchmarkLibrarySnapshot> library;
        std::uint64_t libraryRevision = 0;
        bool refreshFailed = false;
        std::string managedDirectoryPath;

        std::optional<domain::Benchmark> draft;
        bool draftFromLibrary = false;
        bool draftDirty = false;
        domain::CompletenessResult draftCompleteness;

        std::vector<domain::ScenarioId> scenarioCatalogue;
        std::vector<domain::ScenarioResolution> draftResolutions;
        bool resolutionWriteFailed = false;
    };
}
