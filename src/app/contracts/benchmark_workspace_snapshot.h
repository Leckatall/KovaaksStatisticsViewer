#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_ids.h"
#include "data/interfaces/benchmark_library_snapshot.h"
#include "i_benchmark_tracking_use_case.h"
#include "playtime_view.h"

namespace ksv::application {
    enum class BenchmarkChoiceClassification { Trackable, Incomplete, Invalid, Unsupported, MigrationFailed };

    struct BenchmarkChoice {
        std::string filename;
        std::optional<domain::BenchmarkId> id;
        std::string displayName;
        BenchmarkChoiceClassification classification = BenchmarkChoiceClassification::Invalid;
        std::optional<int> schemaVersion;
        std::optional<data::BenchmarkMigrationFailure> migrationFailure;
        bool selectable = false;
        // Index in the recent-benchmarks list, 0 being the most recent; empty when never viewed or created.
        std::optional<std::size_t> recency;
    };

    // The evaluated benchmark keeps the full rank history so a range can carry in the preceding rank.
    struct RankRangeView {
        // Set when the range resolved to calendar dates; the X axis spans exactly this.
        std::optional<DayInterval> interval;
        // An anchored preset found no point of its series to end at: nothing is shown rather than borrowing an anchor.
        bool noRelevantHistory = false;
        // With the anchor flag on, the day of the benchmark's latest member run, for both rank views.
        // `interval` is already cut back to it; a range without one (all history) must still end there
        // instead of at the shared extent.
        std::optional<std::chrono::sys_days> anchoredEnd;

        bool operator==(const RankRangeView &) const = default;
    };

    // Library choices, selection and the evaluated benchmark share a library revision. Consumers
    // read the snapshot once after onChanged and use the reference only for that revision.
    struct BenchmarkWorkspaceSnapshot {
        std::uint64_t libraryRevision = 0;
        std::vector<BenchmarkChoice> choices;
        std::optional<domain::BenchmarkId> selectedId;
        std::string lastKnownSelectedDisplayName;
        BenchmarkTrackingState availability = BenchmarkTrackingState::NoSelection;
        PlaytimeView playtime;
        // Of averageRankHistory.
        RankRangeView rank;
        // Of recentAverageRankHistory.
        RankRangeView recentRank;
        std::optional<domain::Benchmark> benchmark;
        // The range-sliced part of the committed averaging window's playtime series.
        PlaytimeSeries visiblePlaytime;
    };
}
