#pragma once

#include <vector>

#include "app/contracts/series_config.h"
#include "domain/scenario_perf.h"

namespace ksv::application {
    struct BucketedRun {
        std::vector<float> times;
        std::vector<double> score;
        std::vector<double> shots;
        std::vector<double> hits;
        std::vector<double> kills;
        std::vector<double> dmg;

        [[nodiscard]] const std::vector<double> &valuesFor(PrimitiveMetric metric) const;
    };

    [[nodiscard]] BucketedRun bucketRun(const domain::ScenarioPerf &perf);
}
