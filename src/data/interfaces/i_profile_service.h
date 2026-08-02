//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PROFILE_SEVICE_H
#define KOVAAKSSTATSVIEWER_I_PROFILE_SEVICE_H
#include <vector>

#include "domain/scenario_perf.h"

namespace ksv::application {
    class IProfileService {
    public:
        virtual ~IProfileService() = default;

        virtual void generateProfileFromDirectory() = 0;

        [[nodiscard]] virtual std::vector<domain::ScenarioId> getScenarioList() const = 0;
    };
}
#endif
