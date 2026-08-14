//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_SERIES_H
#define KOVAAKSSTATSVIEWER_GRAPH_SERIES_H

#include <map>
#include <vector>

#include "graph_column.h"

namespace ksv::application {
    // Resampled to 1-second intervals; columns[*][i] aligns with times[i]. Safe for lockstep indexing.
    struct GraphSeries {
        std::vector<float> times;
        std::map<ColumnId, std::vector<float>> columns;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_SERIES_H
