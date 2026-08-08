//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PERF_COLUMN_BUILDER_H
#define KOVAAKSSTATSVIEWER_PERF_COLUMN_BUILDER_H

#include <map>
#include <vector>

#include "domain/scenario_perf.h"
#include "graph_column.h"

namespace ksv::application {
    // Whole-second-resampled columns for one run. columns[*][i] is the value
    // for times[i]; every column has exactly one entry per second in
    // [0, times.back()], so callers can index them in lockstep without
    // bounds-checking each column separately.
    struct GraphSeries {
        std::vector<float> times;
        std::map<ColumnId, std::vector<float>> columns;
    };

    // Resamples a run's raw per-tick data into GraphSeries and derives every
    // plotted column from it.
    class PerfColumnBuilder {
    public:
        [[nodiscard]] static GraphSeries build(const domain::ScenarioPerf &perf);
    };
}

#endif //KOVAAKSSTATSVIEWER_PERF_COLUMN_BUILDER_H
