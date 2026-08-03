//
// Created by Lecka on 01/08/2026.
//

#include "session_controller.h"

namespace ksv::application {
    SessionController::SessionController(std::shared_ptr<ISettingsService> settings_service,
                                         std::shared_ptr<IProfileService> profile_service,
                                         QObject *parent) : ISessionController(parent),
                                                            m_settings_service(std::move(settings_service)),
                                                            m_profile_service(std::move(profile_service)) {
        setCurrentPerfToLatest();
        m_profile_service->onProfileChanged([this] { setCurrentPerfToLatest(); });
    }

    std::string SessionController::getKovaaksDir() const {
        return m_settings_service->getKovaaksDir();
    }

    std::vector<domain::ScenarioId> SessionController::getScenarioList() {
        return m_profile_service->getScenarioList();
    }

    void SessionController::generateProfileFromDirectory() const {
        m_profile_service->generateProfileFromDirectory();
    }

    void SessionController::setCurrentPerf(const domain::ScenarioPerf &perf) {
        if (m_current_perf.run_id == perf.run_id) return;
        m_current_perf = perf;
        emit currentPerfChanged();
    }

    void SessionController::setCurrentPerf(const std::string &filename) {
        setCurrentPerf(m_profile_service->getPerf(filename));
    }

    void SessionController::setCurrentPerfToLatest() {
        setCurrentPerf(m_profile_service->getLatestPerf());
    }
}
