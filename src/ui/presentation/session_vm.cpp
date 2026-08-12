//
// Created by Lecka on 30/07/2026.
//

#include "session_vm.h"

namespace ksv::presentation {
    SessionViewModel::SessionViewModel(
        std::shared_ptr<application::ISessionController> session_controller,
        QObject *parent) : QObject(parent),
                           m_session_controller(std::move(session_controller)) {
        connect(m_session_controller.get(), &application::ISessionController::profileChanged,
                this, &SessionViewModel::updateScenarioHashMap);
        connect(m_session_controller.get(), &application::ISessionController::buildStarted,
                this, [this] { setBuildInProgress(true); });
        connect(m_session_controller.get(), &application::ISessionController::buildFinished,
                this, [this] { setBuildInProgress(false); });
        connect(m_session_controller.get(), &application::ISessionController::buildProgress,
                this, [this](const int done, const int total) {
                    m_build_done = done;
                    m_build_total = total;
                    emit profileBuildChanged();
                });

        // App builds the profile before it builds the view models, so a build can
        // already be running by the time this connects.
        m_build_in_progress = m_session_controller->isBuildInProgress();

        updateScenarioHashMap();
    }

    void SessionViewModel::setBuildInProgress(const bool in_progress) {
        m_build_in_progress = in_progress;
        m_build_done = 0;
        m_build_total = 0;
        emit profileBuildChanged();
    }

    void SessionViewModel::updateScenarioHashMap() {
        std::vector<domain::ScenarioId> scenarios = m_session_controller->getScenarioList();
        QMap<QString, QString> scenario_list;
        for (const auto &scenario: scenarios) {
            scenario_list[QString::fromStdString(scenario.hash)] = QString::fromStdString(scenario.name);
        }
        if (m_scenario_hash_to_name != scenario_list) emit scenario_list_changed();
        m_scenario_hash_to_name = scenario_list;
    }

    QStringList SessionViewModel::getScenarioList() {
        return m_scenario_hash_to_name.values();
    }
}
