#pragma once

#include <optional>
#include <vector>

#include "series_config.h"

namespace ksv::application {
    struct ResolvedGraphSeries {
        // TODO: Series should be created with x-axis
        SeriesConfig config;
        std::optional<std::vector<double>> values; //TODO: Why are series values optional??
    };

    struct ResolvedGraph {
        std::vector<float> times;
        std::vector<ResolvedGraphSeries> series;
    };
}
