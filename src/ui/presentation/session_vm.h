//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SESSION_VM_H
#define KOVAAKSSTATSVIEWER_SESSION_VM_H

#include <QtCore>

#include "app/usecases/i_session_controller.h"

namespace ksv::presentation {
    class SessionViewModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QStringList scenario_list READ getScenarioList NOTIFY scenario_list_changed)
        Q_PROPERTY(bool profileBuildInProgress READ profileBuildInProgress NOTIFY profileBuildChanged)
        Q_PROPERTY(double profileBuildProgress READ profileBuildProgress NOTIFY profileBuildChanged)
        Q_PROPERTY(int profileBuildDone READ profileBuildDone NOTIFY profileBuildChanged)
        Q_PROPERTY(int profileBuildTotal READ profileBuildTotal NOTIFY profileBuildChanged)
        Q_PROPERTY(bool profileImportBusy READ profileImportBusy NOTIFY profileImportChanged)
        Q_PROPERTY(QString profileImportMode READ profileImportMode NOTIFY profileImportChanged)
        Q_PROPERTY(QString profileImportPhase READ profileImportPhase NOTIFY profileImportChanged)
        Q_PROPERTY(QString profileImportOutcome READ profileImportOutcome NOTIFY profileImportChanged)
        Q_PROPERTY(double profileImportProgress READ profileImportProgress NOTIFY profileImportChanged)
        Q_PROPERTY(int profileImportDone READ profileImportDone NOTIFY profileImportChanged)
        Q_PROPERTY(int profileImportTotal READ profileImportTotal NOTIFY profileImportChanged)
        Q_PROPERTY(int profileImportAdded READ profileImportAdded NOTIFY profileImportChanged)
        Q_PROPERTY(int profileImportEnriched READ profileImportEnriched NOTIFY profileImportChanged)
        Q_PROPERTY(QVariantList profileImportIssues READ profileImportIssues NOTIFY profileImportChanged)
        Q_PROPERTY(bool profileImportSaveFailed READ profileImportSaveFailed NOTIFY profileImportChanged)

    public:
        explicit SessionViewModel(std::shared_ptr<application::ISessionController> session_controller,
                                  QObject *parent = nullptr);

        void updateScenarioHashMap();

        Q_INVOKABLE [[nodiscard]] QStringList getScenarioList();

        Q_INVOKABLE void generateProfile() { m_session_controller->generateProfileFromDirectory(); }
        Q_INVOKABLE void refreshProfile() { m_session_controller->refreshProfile(); }
        [[nodiscard]] bool profileImportBusy() const { return m_import_state.busy; }
        [[nodiscard]] QString profileImportMode() const {
            return m_import_state.mode == application::ProfileImportMode::Refresh ? "refresh" : "rebuild";
        }
        [[nodiscard]] QString profileImportPhase() const;
        [[nodiscard]] QString profileImportOutcome() const;
        [[nodiscard]] double profileImportProgress() const {
            return m_import_state.total == 0 ? 0.0 : static_cast<double>(m_import_state.done) / m_import_state.total;
        }
        [[nodiscard]] int profileImportDone() const { return static_cast<int>(m_import_state.done); }
        [[nodiscard]] int profileImportTotal() const { return static_cast<int>(m_import_state.total); }
        [[nodiscard]] int profileImportAdded() const { return static_cast<int>(m_import_state.summary.added); }
        [[nodiscard]] int profileImportEnriched() const { return static_cast<int>(m_import_state.summary.enriched); }
        [[nodiscard]] QVariantList profileImportIssues() const;
        [[nodiscard]] bool profileImportSaveFailed() const { return m_import_state.summary.save_failed; }

        [[nodiscard]] domain::Run getCurrentRun() const { return m_session_controller->getCurrentRun(); }
        Q_INVOKABLE [[nodiscard]] QString getCurrentRunScenario() const { return getCurrentRun().run_id.scenario_id.name.data(); }

        [[nodiscard]] bool profileBuildInProgress() const { return m_build_in_progress; }

        // 0..1. Stays 0 until the first per-file report arrives, so a build whose
        // directory scan has not produced a file count yet reads as "just started".
        [[nodiscard]] double profileBuildProgress() const {
            return m_build_total == 0 ? 0.0 : static_cast<double>(m_build_done) / m_build_total;
        }

        // Files finished and files found so far; both stay 0 until the first per-file report.
        [[nodiscard]] int profileBuildDone() const { return m_build_done; }
        [[nodiscard]] int profileBuildTotal() const { return m_build_total; }

    signals:
        void scenario_list_changed();
        void profileBuildChanged();
        void profileImportChanged();

    private:
        void setBuildInProgress(bool in_progress);

        std::shared_ptr<application::ISessionController> m_session_controller;
        QMap<QString, QString> m_scenario_hash_to_name;
        bool m_build_in_progress = false;
        int m_build_done = 0;
        int m_build_total = 0;
        application::ProfileImportState m_import_state;
    };
}

#endif //KOVAAKSSTATSVIEWER_SESSION_VM_H
