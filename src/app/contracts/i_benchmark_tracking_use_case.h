#ifndef KOVAAKSSTATSVIEWER_I_BENCHMARK_TRACKING_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_BENCHMARK_TRACKING_USE_CASE_H

#include <functional>
#include <optional>

#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_projection.h"
#include "graph_presentation.h"

namespace ksv::application {
    enum class BenchmarkTrackingState { NoSelection, Unavailable, Ready };

    struct BenchmarkWorkspaceSnapshot;

    // Asynchronous-ready by construction: no command returns a projection. A consumer selects,
    // waits for onChanged, then reads snapshot(). Moving evaluation onto a worker later would
    // change only when the callback fires, not the shape any caller depends on.
    class IBenchmarkTrackingUseCase {
    public:
        virtual ~IBenchmarkTrackingUseCase() = default;

        // An explicit choice; it is also remembered as the benchmark to restore on the next launch.
        virtual void select(const domain::BenchmarkId &id) = 0;
        virtual void clearSelection() = 0;
        // Only acts while nothing is selected: selects the remembered benchmark if it is still
        // selectable, otherwise the only selectable benchmark when exactly one exists. Otherwise
        // (a selection already exists, or no single candidate) it changes nothing and does not notify.
        virtual void selectDefault() = 0;
        // Valid until the next select/clearSelection or service callback; never retained by callers.
        [[nodiscard]] virtual const BenchmarkWorkspaceSnapshot &snapshot() const = 0;
        // The selected id is kept even while the state is Unavailable, so a benchmark that
        // reappears on the next refresh comes back without the user reselecting it.
        [[nodiscard]] virtual std::optional<domain::BenchmarkId> selected() const = 0;
        [[nodiscard]] virtual BenchmarkTrackingState state() const = 0;
        // nullptr unless state() is Ready.
        [[nodiscard]] virtual const domain::BenchmarkProjection *projection() const = 0;
        virtual void onChanged(std::function<void()> callback) = 0;
        // Fired instead of onChanged when only the playtime graph's window or range moved: the
        // snapshot's projection summaries and rank history are the ones the last onChanged delivered.
        virtual void onPlaytimeChanged(std::function<void()> callback) = 0;
        // Likewise for the rank graph's range: only the snapshot's `rank` view moved.
        virtual void onRankChanged(std::function<void()> callback) = 0;
        // `graph` is BenchmarkRank or BenchmarkPlaytime; selecting another benchmark never changes it.
        [[nodiscard]] virtual GraphPresentation presentation(GraphKey graph) const = 0;
        // Fires for a committed style change of either benchmark graph and nothing else: no
        // evaluation, projection or range moved.
        virtual void onPresentationChanged(std::function<void()> callback) = 0;
    };
}

#endif
