#pragma once

#include <optional>
#include <vector>

#include "app/contracts/series_config.h"
#include "domain/run.h"

namespace ksv::application {
    class IAverageLineUseCase {
    public:
        virtual ~IAverageLineUseCase() = default;

        [[nodiscard]] virtual std::optional<std::vector<double> > evaluate(
            const domain::Run &referenceRun, const Expression &expression) const = 0;
    };
}
