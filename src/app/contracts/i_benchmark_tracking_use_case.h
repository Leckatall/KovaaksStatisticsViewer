#ifndef KOVAAKSSTATSVIEWER_I_BENCHMARK_TRACKING_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_BENCHMARK_TRACKING_USE_CASE_H

#include <functional>
#include <optional>

#include "benchmarks/benchmark_ids.h"
#include "benchmarks/benchmark_projection.h"

namespace ksv::application {
    enum class BenchmarkTrackingState { NoSelection, Unavailable, Ready };

    // Asynchronous-ready by construction: no command returns a projection. A consumer selects,
    // waits for onChanged, then reads projection(). Moving evaluation onto a worker later would
    // change only when the callback fires, not the shape any caller depends on.
    class IBenchmarkTrackingUseCase {
    public:
        virtual ~IBenchmarkTrackingUseCase() = default;

        virtual void select(const domain::BenchmarkId &id) = 0;
        virtual void clearSelection() = 0;
        // The selected id is kept even while the state is Unavailable, so a benchmark that
        // reappears on the next refresh comes back without the user reselecting it.
        [[nodiscard]] virtual std::optional<domain::BenchmarkId> selected() const = 0;
        [[nodiscard]] virtual BenchmarkTrackingState state() const = 0;
        // nullptr unless state() is Ready.
        [[nodiscard]] virtual const domain::BenchmarkProjection *projection() const = 0;
        virtual void onChanged(std::function<void()> callback) = 0;
    };
}

#endif
