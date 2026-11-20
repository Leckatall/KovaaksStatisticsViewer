#ifndef KOVAAKSSTATSVIEWER_I_BENCHMARK_TRACKING_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_BENCHMARK_TRACKING_USE_CASE_H

#include <functional>
#include <optional>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_ids.h"
#include "graph_presentation.h"

namespace ksv::application {
    enum class BenchmarkTrackingState { NoSelection, Unavailable, Ready };

    struct BenchmarkWorkspaceSnapshot;

    // A consumer selects, waits for onChanged, then reads snapshot(). Moving evaluation onto a
    // worker later would change only when the callback fires.
    class IBenchmarkTrackingUseCase {
    public:
        virtual ~IBenchmarkTrackingUseCase() = default;

        // An explicit choice; it moves to the front of the recent-benchmarks list, which is what
        // the next launch restores from. Notifies once.
        virtual void select(const domain::BenchmarkId &id) = 0;
        // Leaves the recent-benchmarks list untouched.
        virtual void clearSelection() = 0;
        // Only acts while nothing is selected: selects the first benchmark in the recent list that
        // is still selectable, otherwise the only selectable benchmark when exactly one exists.
        // Otherwise (a selection already exists, or no single candidate) it changes nothing and
        // does not notify.
        virtual void selectDefault() = 0;
        // Valid until the next select/clearSelection or service callback; never retained by callers.
        [[nodiscard]] virtual const BenchmarkWorkspaceSnapshot &snapshot() const = 0;
        // The selected id is kept even while the state is Unavailable, so a benchmark that
        // reappears on the next refresh comes back without the user reselecting it.
        [[nodiscard]] virtual std::optional<domain::BenchmarkId> selected() const = 0;
        [[nodiscard]] virtual BenchmarkTrackingState state() const = 0;
        // nullptr unless state() is Ready.
        [[nodiscard]] virtual const domain::Benchmark *benchmark() const = 0;
        virtual void onChanged(std::function<void()> callback) = 0;
        // Fired instead of onChanged when only the playtime graph's window or range moved.
        virtual void onPlaytimeChanged(std::function<void()> callback) = 0;
        // Likewise for a rank graph's range: only the snapshot view of the named graph moved -
        // `rank` for BenchmarkRank, `recentRank` for BenchmarkRecentRank.
        virtual void onRankChanged(std::function<void(GraphKey)> callback) = 0;
        // `graph` is BenchmarkRank, BenchmarkRecentRank or BenchmarkPlaytime; selecting another
        // benchmark never changes it.
        [[nodiscard]] virtual GraphPresentation presentation(GraphKey graph) const = 0;
        // Fires only for a committed style change of a benchmark graph.
        virtual void onPresentationChanged(std::function<void()> callback) = 0;
    };
}

#endif
