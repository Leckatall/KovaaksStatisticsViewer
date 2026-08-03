//
// Created by Lecka on 30/07/2026.
//

#include "session_vm.h"

namespace ksv::presentation {
    SessionViewModel::SessionViewModel(
        std::shared_ptr<application::ISessionController> session_controller,
        QObject *parent) : QObject(parent),
                           m_session_controller(std::move(session_controller)) {
        updateScenarioHashMap();
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
