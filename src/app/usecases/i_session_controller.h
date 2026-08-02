//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
#define KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
#include <memory>
#include <string>
#include <vector>

#include "domain/scenario_perf.h"
#include "../../data/interfaces/i_profile_service.h"

namespace ksv::application {
    class ISessionController {
    public:
        virtual ~ISessionController() = default;
        virtual std::vector<domain::ScenarioId> getScenarioList() = 0;
        [[nodiscard]] virtual std::string getKovaaksDir() const = 0;

        virtual void setProfileService(std::shared_ptr<IProfileService> profile_service) = 0;
        virtual void generateProfileFromDirectory() const = 0;
    };
}


#endif //KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
