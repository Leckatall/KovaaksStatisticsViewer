//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H
#define KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H

#include <QObject>
#include <QThread>
#include <QTimer>

#include "profile_build_worker.h"
#include "run.h"
#include "data/interfaces/i_file_service.h"
#include "data/interfaces/i_profile_service.h"
#include "interfaces/i_settings_service.h"
#include "usecases/i_session_controller.h"

namespace ksv::application {
    class SessionController : public ISessionController {
        Q_OBJECT

    public:
        SessionController(std::shared_ptr<ISettingsService> settings_service,
                          std::shared_ptr<IProfileService> profile_service,
                          std::shared_ptr<IFileService> file_service,
                          std::shared_ptr<data::IRunIngestor> ingestor, QObject *parent = nullptr);
        SessionController(std::shared_ptr<ISettingsService>, std::shared_ptr<IProfileService>,
            std::shared_ptr<IFileService>, std::shared_ptr<data::IProfileImporter>, QObject *parent = nullptr);

        ~SessionController() override;

        std::vector<domain::ScenarioId> getScenarioList() override;

        void generateProfileFromDirectory() override;
        void refreshProfile(ProfileImportReason reason = ProfileImportReason::Manual) override;
        [[nodiscard]] ProfileImportState profileImportState() const override { return m_import_state; }

        void setCurrentRun(const domain::Run& run) override;

        void setCurrentPerf(const std::string& filename) override;

        void setCurrentRun(const domain::ScenarioRunId& run_id) override;

        void setCurrentRunToLatest() override;

        [[nodiscard]] domain::Run getCurrentRun() const override { return m_current_run; }

        [[nodiscard]] bool isBuildInProgress() const override { return m_build_in_flight; }

    signals:
        void buildRequested(ProfileImportRequest request);

    private:
        void startBuild();
        void startImport(ProfileImportMode, ProfileImportReason);
        void onBuildFinished(ProfileImportResult result);
        void refreshCurrentRun(const ProfileImportResult &);

        std::shared_ptr<ISettingsService> m_settings_service;
        std::shared_ptr<IProfileService> m_profile_service;
        std::shared_ptr<IFileService> m_file_service;
        domain::Run m_current_run;

        QThread m_build_thread;
        ProfileBuildWorker* m_build_worker;
        bool m_build_in_flight = false;
        bool m_rebuild_requested = false;
        bool m_refresh_requested = false;
        ProfileImportReason m_pending_reason = ProfileImportReason::Live;
        ProfileImportRequest m_active_request;
        ProfileImportState m_import_state;
        QTimer m_retry;
        int m_retry_attempt = 0;
        std::uint64_t m_selection_revision = 0;
        std::uint64_t m_started_selection_revision = 0;
        bool m_accepting = false;
        std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    };
}

#endif //KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H
