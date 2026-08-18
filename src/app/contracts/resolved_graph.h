#pragma once

#include <optional>
#include <vector>

#include "series_config.h"

namespace ksv::application {
    struct ResolvedGraphSeries {
        SeriesConfig config;
        std::optional<std::vector<double>> values;
    };

    struct ResolvedGraph {
        std::vector<float> times;
        std::vector<ResolvedGraphSeries> series;
    };
}
