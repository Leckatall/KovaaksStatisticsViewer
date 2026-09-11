#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "benchmark_resolution_snapshot.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_resolution.h"
#include "data/interfaces/benchmark_library_snapshot.h"
#include "run.h"

namespace ksv::application {
    // One coherent revision of the Benchmark Manager's accepted and derived application state: the
    // accepted library listing, its revision, refresh diagnostics, and the profile/benchmark
    // resolution join. The editable working copy lives in presentation, never here. Consumers read
    // this once after an onChanged notification and never retain the reference.
    struct BenchmarkManagerState {
        std::optional<data::BenchmarkLibrarySnapshot> library;
        std::uint64_t libraryRevision = 0;
        bool refreshFailed = false;
        std::string managedDirectoryPath;

        std::vector<domain::ScenarioId> scenarioCatalogue;
        std::map<domain::BenchmarkId, std::vector<domain::ScenarioResolution>> resolutions;
        std::map<domain::BenchmarkId, AutomaticMappingWriteError> automaticWriteFailures;
        bool resolutionWriteFailed = false;
    };
}
