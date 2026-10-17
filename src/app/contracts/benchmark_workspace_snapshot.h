#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_projection.h"
#include "data/interfaces/benchmark_library_snapshot.h"
#include "i_benchmark_tracking_use_case.h"
#include "playtime_view.h"

namespace ksv::application {
    enum class BenchmarkChoiceClassification { Trackable, Incomplete, Invalid, Unsupported };

    struct BenchmarkChoice {
        std::string filename;
        std::optional<domain::BenchmarkId> id;
        std::string displayName;
        BenchmarkChoiceClassification classification = BenchmarkChoiceClassification::Invalid;
        std::optional<int> schemaVersion;
        bool selectable = false;
        // Index in the recent-benchmarks list, 0 being the most recent; empty when never viewed or created.
        std::optional<std::size_t> recency;
    };

    // What the rank graph's committed range resolved to. The rank history itself stays whole in the
    // projection, so the rank established before a range can be carried into it.
    struct RankRangeView {
        // Set when the range resolved to calendar dates; the X axis spans exactly this.
        std::optional<DayInterval> interval;
        // An anchored preset found no contributing run: nothing is shown rather than borrowing an anchor.
        bool noRelevantHistory = false;

        bool operator==(const RankRangeView &) const = default;
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
        std::optional<data::LoadedBenchmark> selectedLoaded;
        // Its `rollingPlaytime` is already the range-visible part of the committed window's series.
        std::optional<domain::BenchmarkProjection> projection;
        PlaytimeView playtime;
        RankRangeView rank;
    };
}
