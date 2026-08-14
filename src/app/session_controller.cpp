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

    std::vector<ScenarioSummary> SessionController::getScenarioSummaries() const {
        std::vector<ScenarioSummary> summaries;
        const auto scenarios = m_profile_service->getScenarioList();
        summaries.reserve(scenarios.size());
        for (const auto &scenario: scenarios) {
            ScenarioSummary summary;
            summary.scenario_id = scenario;
            summary.run_count = static_cast<int>(m_profile_service->getRunCount(scenario).value_or(0));
            summary.total_time_seconds = m_profile_service->getTotalTime(scenario).value_or(0.0);
            if (m_profile_service->getLastRunTime(scenario)) {
                summary.last_played = m_profile_service->getLastRunTime(scenario).value();
            } else {
                std::cerr << "No last run time for scenario '" << scenario.name << "'" << std::endl;
            }

            summaries.push_back(summary);
        }
        return summaries;
    }

    std::vector<domain::RunPerformance> SessionController::getRunsForScenario(const domain::ScenarioId &scenario) const {
        const auto count = m_profile_service->getRunCount(scenario).value_or(0);
        const auto perfs = m_profile_service->getMostRecentPerfs(scenario, count);

        std::vector<domain::RunPerformance> summaries;
        summaries.reserve(perfs.size());
        for (auto it = perfs.rbegin(); it != perfs.rend(); ++it) {
            summaries.push_back({it->run_id, it->getCompletionData()});
        }
        return summaries;
    }

    std::vector<domain::RunPerformance> SessionController::getRecentRuns(const std::size_t count) const {
        const auto perfs = m_profile_service->getRecentRuns(count);

        std::vector<domain::RunPerformance> summaries;
        summaries.reserve(perfs.size());
        for (const auto &perf: perfs) {
            summaries.push_back({perf.run_id, perf.getCompletionData()});
        }
        return summaries;
    }
}
