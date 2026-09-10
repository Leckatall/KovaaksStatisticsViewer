#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "benchmark_library_snapshot.h"
#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_projection.h"
#include "i_benchmark_tracking_use_case.h"

namespace ksv::application {
    enum class BenchmarkChoiceClassification { Trackable, Incomplete, Invalid, Unsupported };

    struct BenchmarkChoice {
        std::string filename;
        std::optional<domain::BenchmarkId> id;
        std::string displayName;
        BenchmarkChoiceClassification classification = BenchmarkChoiceClassification::Invalid;
        std::optional<int> schemaVersion;
        bool selectable = false;
    };

    // One coherent revision of the tracking workspace: library choices, selection, and the selected
    // definition/completeness/projection all belong to the same `libraryRevision`. Consumers read it
    // once after an onChanged notification and never retain the reference.
    struct BenchmarkWorkspaceSnapshot {
        std::uint64_t libraryRevision = 0;
        std::vector<BenchmarkChoice> choices;
        std::optional<domain::BenchmarkId> selectedId;
        std::string lastKnownSelectedDisplayName;
        BenchmarkTrackingState availability = BenchmarkTrackingState::NoSelection;
        std::optional<LoadedBenchmark> selectedLoaded;
        std::optional<domain::BenchmarkProjection> projection;
    };
}
