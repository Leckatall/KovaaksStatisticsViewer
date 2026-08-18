#pragma once

#include <memory>

#include "app/contracts/i_average_line_use_case.h"
#include "data/interfaces/i_profile_service.h"

namespace ksv::application {
    class AverageLineUseCase final : public IAverageLineUseCase {
    public:
        explicit AverageLineUseCase(std::shared_ptr<IProfileService> profileService);

        [[nodiscard]] std::optional<std::vector<double> > evaluate(
            const domain::ScenarioPerf &referenceRun, const Expression &expression) const override;

    private:
        std::shared_ptr<IProfileService> m_profileService;
    };
}
