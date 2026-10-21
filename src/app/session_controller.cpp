//
// Created by Lecka on 01/08/2026.
//

#include "session_controller.h"

#include <utility>
#include <algorithm>
#include "profile_importer.h"

namespace ksv::application {
    SessionController::SessionController(std::shared_ptr<ISettingsService> settings,
            std::shared_ptr<IProfileService> profiles, std::shared_ptr<IFileService> files,
            std::shared_ptr<data::IRunIngestor> ingestor, QObject *parent)
        : SessionController(std::move(settings), std::move(profiles), files,
            std::make_shared<data::ProfileImporter>(files, std::move(ingestor)), parent) {}

    SessionController::SessionController(std::shared_ptr<ISettingsService> settings,
            std::shared_ptr<IProfileService> profiles, std::shared_ptr<IFileService> files,
            std::shared_ptr<data::IProfileImporter> importer, QObject *parent)
        : ISessionController(parent), m_settings_service(std::move(settings)),
          m_profile_service(std::move(profiles)), m_file_service(std::move(files)) {
        qRegisterMetaType<domain::UserProfile>();
        qRegisterMetaType<ProfileImportRequest>();
        qRegisterMetaType<ProfileImportResult>();
        qRegisterMetaType<ProfileImportPhase>();
        m_build_worker = new ProfileBuildWorker(std::move(importer));
        m_build_worker->moveToThread(&m_build_thread);
        connect(&m_build_thread, &QThread::finished, m_build_worker, &QObject::deleteLater);
        connect(this, &SessionController::buildRequested, m_build_worker, &ProfileBuildWorker::build);
        connect(m_build_worker, &ProfileBuildWorker::finished, this, &SessionController::onBuildFinished);
        connect(m_build_worker, &ProfileBuildWorker::progress, this,
            [this](ProfileImportPhase phase, int done, int total) {
                m_import_state.phase = phase;
                m_import_state.done = done;
                m_import_state.total = total;
                emit buildProgress(done, total);
                emit profileImportStateChanged();
            });
        m_build_thread.start();
        m_retry.setSingleShot(true);
        connect(&m_retry, &QTimer::timeout, this, [this] {
            startImport(ProfileImportMode::Refresh, ProfileImportReason::Retry);
        });
        SessionController::setCurrentRunToLatest();
        const auto lifetime = std::weak_ptr(m_lifetime);
        m_profile_service->onProfileChanged([this, lifetime] {
            if (lifetime.expired()) return;
            if (m_accepting) return;
            setCurrentRunToLatest();
            emit profileChanged();
        });
        m_profile_service->onBuildRequested([this] { startBuild(); });
        m_profile_service->onRefreshRequested([this](ProfileImportReason reason) { refreshProfile(reason); });
    }

    SessionController::~SessionController() {
        // quit() cannot interrupt a build already running in the worker slot, so a
        // shutdown mid-build waits it out rather than tearing the thread down under it.
        m_retry.stop();
        m_profile_service->onBuildRequested({});
        m_profile_service->onRefreshRequested({});
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
        m_retry.stop();
        m_retry_attempt = 0;
        startImport(ProfileImportMode::Rebuild, ProfileImportReason::Manual);
    }

    void SessionController::refreshProfile(ProfileImportReason reason) {
        // A Live arrival inside a retry window (e.g. a run's .csv landing just after its .perf failed
        // transiently) must not skip the still-unchanged failure that window exists to re-read.
        const bool retry_window = m_retry.isActive() ||
            (m_build_in_flight && (m_active_request.reason == ProfileImportReason::Live ||
                                   m_active_request.reason == ProfileImportReason::Retry)) ||
            (m_refresh_requested && m_pending_reason == ProfileImportReason::Retry);
        if (reason == ProfileImportReason::Live && retry_window) reason = ProfileImportReason::Retry;
        else m_retry_attempt = 0;
        m_retry.stop();
        startImport(ProfileImportMode::Refresh, reason);
    }

    void SessionController::startImport(ProfileImportMode mode, ProfileImportReason reason) {
        if (m_build_in_flight) {
            if (mode == ProfileImportMode::Rebuild) {
                m_rebuild_requested = true;
                m_refresh_requested = false;
            }
            else {
                if (!m_refresh_requested || m_pending_reason != ProfileImportReason::Manual)
                    m_pending_reason = reason;
                m_refresh_requested = true;
            }
            return;
        }
        m_build_in_flight = true;
        m_active_request = m_profile_service->beginImport(mode, reason);
        m_started_selection_revision = m_selection_revision;
        m_import_state = {};
        m_import_state.mode = mode;
        m_import_state.phase = ProfileImportPhase::Scanning;
        m_import_state.busy = true;
        emit buildStarted();
        emit profileImportStateChanged();
        emit buildRequested(m_active_request);
    }

    void SessionController::refreshCurrentRun(const ProfileImportResult &result) {
        const bool latest = m_active_request.mode == ProfileImportMode::Rebuild ||
            m_active_request.reason == ProfileImportReason::Live ||
            m_active_request.reason == ProfileImportReason::Retry ||
            (m_active_request.reason == ProfileImportReason::Startup &&
             m_started_selection_revision == m_selection_revision) ||
            m_current_run.run_id.scenario_id.hash.empty();
        auto id = m_current_run.run_id;
        for (const auto &[old_id, new_id] : result.rekeyed_runs) if (id == old_id) id = new_id;
        const auto selected = latest ? std::optional{m_profile_service->getLatestRun()}
            : m_profile_service->getCurrentRun(id);
        if (selected) {
            m_current_run = *selected;
            emit currentRunChanged();
        }
    }

    void SessionController::onBuildFinished(ProfileImportResult result) {
        m_import_state.phase = ProfileImportPhase::Saving;
        emit profileImportStateChanged();
        m_accepting = true;
        const auto outcome = m_profile_service->acceptImport(result);
        m_accepting = false;
        const bool accepted = outcome == ProfileImportOutcome::Succeeded || outcome == ProfileImportOutcome::Partial;
        if (accepted && result.changed) {
            refreshCurrentRun(result);
            emit profileChanged();
        }
        m_build_in_flight = false;
        const auto completed_reason = m_active_request.reason;
        m_active_request.baseline.reset();
        m_active_request.known_aliases.clear();
        if (std::exchange(m_rebuild_requested, false)) {
            startImport(ProfileImportMode::Rebuild, ProfileImportReason::Manual);
            return;
        }
        if (std::exchange(m_refresh_requested, false)) {
            const auto reason = std::exchange(m_pending_reason, ProfileImportReason::Live);
            startImport(ProfileImportMode::Refresh, reason);
            return;
        }
        m_import_state.busy = false;
        m_import_state.phase = ProfileImportPhase::Idle;
        m_import_state.outcome = outcome;
        m_import_state.summary = std::move(result.summary);
        emit buildFinished();
        emit profileImportStateChanged();
        const bool retryable = std::ranges::any_of(m_import_state.summary.issues,
            [](const auto &issue) { return issue.retryable; });
        const bool live = completed_reason == ProfileImportReason::Live || completed_reason == ProfileImportReason::Retry;
        if (accepted && retryable && live && m_retry_attempt < 3)
            m_retry.start(250 * (1 << m_retry_attempt++));
    }

    void SessionController::setCurrentRun(const domain::Run &run) {
        ++m_selection_revision;
        if (m_current_run.run_id == run.run_id) return;
        m_current_run = run;
        emit currentRunChanged();
    }

    void SessionController::setCurrentPerf(const std::string &filename) {
        setCurrentRun(m_profile_service->getPerf(filename));
    }

    void SessionController::setCurrentRun(const domain::ScenarioRunId &run_id) {
        if (const auto run = m_profile_service->getCurrentRun(run_id)) {
            setCurrentRun(*run);
        }
    }

    void SessionController::setCurrentRunToLatest() {
        setCurrentRun(m_profile_service->getLatestRun());
    }

}
