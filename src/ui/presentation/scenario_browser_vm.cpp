//
// Created by Lecka on 09/08/2026.
//

#include "scenario_browser_vm.h"

namespace ksv::presentation {
    ScenarioBrowserViewModel::ScenarioBrowserViewModel(
        std::shared_ptr<application::ISessionController> session_controller,
        QObject *parent) : QObject(parent),
                           m_session_controller(std::move(session_controller)),
                           m_model(new ScenarioListModel(this)) {
        connect(m_session_controller.get(), &application::ISessionController::currentPerfChanged,
                this, &ScenarioBrowserViewModel::refresh);
        refresh();
    }

    void ScenarioBrowserViewModel::refresh() {
        m_all_summaries = m_session_controller->getScenarioSummaries();
        applyFilter();
    }

    void ScenarioBrowserViewModel::setSearchText(const QString &text) {
        m_search_text = text;
        applyFilter();
    }

    void ScenarioBrowserViewModel::applyFilter() {
        if (m_search_text.isEmpty()) {
            m_model->setSummaries(m_all_summaries);
            return;
        }

        std::vector<application::ScenarioSummary> filtered;
        for (const auto &summary: m_all_summaries) {
            if (QString::fromStdString(summary.scenario_id.name).contains(m_search_text, Qt::CaseInsensitive))
                filtered.push_back(summary);
        }
        m_model->setSummaries(std::move(filtered));
    }

    void ScenarioBrowserViewModel::activateScenario(const QString &hash, const QString &name) {
        Q_UNUSED(name)
        if (m_active_scenario_hash == hash) return;
        m_active_scenario_hash = hash;
        emit activeScenarioChanged();
    }
}
