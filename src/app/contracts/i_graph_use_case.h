//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
#include <functional>
#include <string>

#include "graph_series.h"
#include "resolved_graph.h"
#include "../../data/interfaces/i_series_config_store.h"

namespace ksv::application {
    class IGraphUseCase {
    public:
        virtual ~IGraphUseCase() = default;

        virtual void load_perf(std::string_view filename) = 0;

        virtual void load_latest_perf() = 0;

        virtual GraphSeries get_series() = 0;

        virtual std::string get_run_label() = 0;

        virtual void onCurrentPerfChanged(std::function<void()> callback) = 0;

        [[nodiscard]] virtual ResolvedGraph get_resolved_graph() { return {}; }
        virtual MutationResult setSeriesEnabled(SeriesRecordReference, bool) { return {}; }
        virtual MutationResult updateBasePresentation(const UpdateBaseSeriesRequest &) { return {}; }
        virtual MutationResult createComputed(const CreateComputedSeriesRequest &) { return {}; }
        virtual MutationResult updateComputed(const UpdateComputedSeriesRequest &) { return {}; }
        virtual MutationResult removeComputed(ComputedSeriesId) { return {}; }
        virtual MutationResult moveSeries(SeriesRecordReference, uint32_t) { return {}; }
        virtual void onSeriesConfigChanged(std::function<void()>) {}
    };
}


#endif //KOVAAKSSTATSVIEWER_I_GRAPH_USE_CASE_H
