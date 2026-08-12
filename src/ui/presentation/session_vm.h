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

    public:
        explicit SessionViewModel(std::shared_ptr<application::ISessionController> session_controller,
                                  QObject *parent = nullptr);

        void updateScenarioHashMap();

        Q_INVOKABLE [[nodiscard]] QStringList getScenarioList();

        Q_INVOKABLE void generateProfile() { m_session_controller->generateProfileFromDirectory(); }

        Q_INVOKABLE [[nodiscard]] domain::ScenarioPerf getCurrentPerf() const { return m_session_controller->getCurrentPerf(); }
        Q_INVOKABLE [[nodiscard]] QString getCurrentPerfScenario() const { return getCurrentPerf().run_id.scenario_id.name.data(); }

        [[nodiscard]] bool profileBuildInProgress() const { return m_build_in_progress; }

        // 0..1. Stays 0 until the first per-file report arrives, so a build whose
        // directory scan has not produced a file count yet reads as "just started".
        [[nodiscard]] double profileBuildProgress() const {
            return m_build_total == 0 ? 0.0 : static_cast<double>(m_build_done) / m_build_total;
        }

    signals:
        void scenario_list_changed();
        void profileBuildChanged();

    private:
        void setBuildInProgress(bool in_progress);

        std::shared_ptr<application::ISessionController> m_session_controller;
        QMap<QString, QString> m_scenario_hash_to_name;
        bool m_build_in_progress = false;
        int m_build_done = 0;
        int m_build_total = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_SESSION_VM_H
