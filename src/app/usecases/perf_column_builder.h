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
    // Resampled to 1-second intervals; columns[*][i] aligns with times[i]. Safe for lockstep indexing.
    struct GraphSeries {
        std::vector<float> times;
        std::map<ColumnId, std::vector<float>> columns;
    };

    class PerfColumnBuilder {
    public:
        [[nodiscard]] static GraphSeries build(const domain::ScenarioPerf &perf);
    };
}

#endif //KOVAAKSSTATSVIEWER_PERF_COLUMN_BUILDER_H
