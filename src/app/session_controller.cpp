//
// Created by Lecka on 01/08/2026.
//

#include "session_controller.h"

#include <qurl.h>

namespace ksv::application {
    SessionController::SessionController(
                                         QObject *parent) : QObject(parent),
                                                            m_settings("Lecka", "KovaaksStatsViewer",this)
                                                             {
    }

    std::string SessionController::getKovaaksDir() const {
        qDebug() << "Kovaaks dir: " << m_settings.value("file/kovaaks", "C:/Program Files(x86)/Steam/steamapps/common/FPSAimTrainer").toUrl().toLocalFile();
        return m_settings.value("file/kovaaks", "C:/Program Files(x86)/Steam/steamapps/common/FPSAimTrainer").toUrl().toLocalFile().toStdString();
    }

    std::vector<domain::ScenarioId> SessionController::getScenarioList() {
        return m_profile_service->getScenarioList();
    }

    void SessionController::setProfileService(std::shared_ptr<IProfileService> profile_service) {
        m_profile_service = std::move(profile_service);
    }

    void SessionController::generateProfileFromDirectory() const {
        m_profile_service->generateProfileFromDirectory();
    }
}
