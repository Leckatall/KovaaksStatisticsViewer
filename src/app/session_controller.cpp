//
// Created by Lecka on 01/08/2026.
//

#include "session_controller.h"

#include <utility>

namespace ksv::application {
    SessionController::SessionController(std::shared_ptr<ISettingsService> settings_service,
                                         std::shared_ptr<IProfileService> profile_service,
                                         std::shared_ptr<IFileService> file_service,
                                         QObject *parent) : ISessionController(parent),
                                                            m_settings_service(std::move(settings_service)),
                                                            m_profile_service(std::move(profile_service)),
                                                            m_file_service(std::move(file_service)) {
        qRegisterMetaType<domain::UserProfile>();

        m_build_worker = new ProfileBuildWorker(m_file_service);
        m_build_worker->moveToThread(&m_build_thread);
        connect(&m_build_thread, &QThread::finished, m_build_worker, &QObject::deleteLater);
        connect(this, &SessionController::buildRequested, m_build_worker, &ProfileBuildWorker::build);
        connect(m_build_worker, &ProfileBuildWorker::finished, this, &SessionController::onBuildFinished);
        connect(m_build_worker, &ProfileBuildWorker::progress, this, &ISessionController::buildProgress);
        m_build_thread.start();

        SessionController::setCurrentPerfToLatest();
        m_profile_service->onProfileChanged([this] {
            setCurrentPerfToLatest();
            emit profileChanged();
        });
        m_profile_service->onBuildRequested([this] { startBuild(); });
    }

    SessionController::~SessionController() {
        // quit() cannot interrupt a build already running in the worker slot, so a
        // shutdown mid-build waits it out rather than tearing the thread down under it.
        m_build_thread.quit();
        m_build_thread.wait();
    }

    std::vector<domain::ScenarioId> SessionController::getScenarioList() {
        return m_profile_service->getScenarioList();
    }

    void SessionController::generateProfileFromDirectory() {
        startBuild();
    }

    void SessionController::startBuild() {
        if (m_build_in_flight) {
            m_rebuild_requested = true;
            return;
        }
        m_build_in_flight = true;
        m_profile_service->beginProfileBuild();
        emit buildStarted();
        emit buildRequested();
    }

    void SessionController::onBuildFinished(const domain::UserProfile &profile) {
        m_build_in_flight = false;
        m_profile_service->applyBuiltProfile(profile);
        if (std::exchange(m_rebuild_requested, false)) {
            startBuild();
            return;
        }
        emit buildFinished();
    }

    void SessionController::setCurrentPerf(const domain::ScenarioPerf &perf) {
        if (m_current_perf.run_id == perf.run_id) return;
        m_current_perf = perf;
        emit currentPerfChanged();
    }

    void SessionController::setCurrentPerf(const std::string &filename) {
        setCurrentPerf(m_profile_service->getPerf(filename));
    }

    void SessionController::setCurrentPerf(const domain::ScenarioRunId &run_id) {
        if (const auto perf = m_profile_service->getRun(run_id)) {
            setCurrentPerf(*perf);
        }
    }

    void SessionController::setCurrentPerfToLatest() {
        setCurrentPerf(m_profile_service->getLatestPerf());
    }

}
