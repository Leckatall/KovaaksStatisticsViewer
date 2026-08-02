//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H
#define KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H

#include <QObject>
#include <QSettings>

#include "scenario_perf.h"
#include "../data/interfaces/i_profile_service.h"
#include "usecases/i_session_controller.h"

namespace ksv::application {
    class SessionController: public QObject, public ISessionController {
        Q_OBJECT
    public:
        explicit SessionController(QObject *parent = nullptr);

        [[nodiscard]] std::string getKovaaksDir() const override;
        std::vector<domain::ScenarioId> getScenarioList() override;

        void setProfileService(std::shared_ptr<IProfileService> profile_service) override;

        void generateProfileFromDirectory() const override;

    private:
        QSettings m_settings;
        std::shared_ptr<IProfileService> m_profile_service{};
    };
}

#endif //KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H
