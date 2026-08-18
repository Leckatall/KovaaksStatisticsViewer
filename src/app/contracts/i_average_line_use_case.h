#pragma once

#include <optional>
#include <vector>

#include "app/contracts/series_config.h"
#include "domain/scenario_perf.h"

namespace ksv::application {
    class IAverageLineUseCase {
    public:
        virtual ~IAverageLineUseCase() = default;

        [[nodiscard]] virtual std::optional<std::vector<double> > evaluate(
            const domain::ScenarioPerf &referenceRun, const Expression &expression) const = 0;
    };
}
