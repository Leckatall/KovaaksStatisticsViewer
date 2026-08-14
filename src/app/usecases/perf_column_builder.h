//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PERF_COLUMN_BUILDER_H
#define KOVAAKSSTATSVIEWER_PERF_COLUMN_BUILDER_H

#include "domain/scenario_perf.h"
#include "contracts/graph_series.h"

namespace ksv::application {
    class PerfColumnBuilder {
    public:
        [[nodiscard]] static GraphSeries build(const domain::ScenarioPerf &perf);
    };
}

#endif //KOVAAKSSTATSVIEWER_PERF_COLUMN_BUILDER_H
