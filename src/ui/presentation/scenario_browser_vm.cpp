//
// Created by Lecka on 09/08/2026.
//

#include "scenario_browser_vm.h"

namespace ksv::presentation {
    ScenarioBrowserViewModel::ScenarioBrowserViewModel(
        std::shared_ptr<application::ISessionController> session_controller,
        QObject *parent) : QObject(parent),
                           m_session_controller(std::move(session_controller)),
                           m_model(new ScenarioListModel(this)),
                           m_run_model(new RunListModel(this)) {
        connect(m_session_controller.get(), &application::ISessionController::currentPerfChanged,
                this, &ScenarioBrowserViewModel::refresh);
        refresh();
    }

    void ScenarioBrowserViewModel::refresh() {
        m_all_summaries = m_session_controller->getScenarioSummaries();
        applyFilter();
        refreshRunModel();
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
        m_active_scenario_name = name;
        if (m_active_scenario_hash == hash) return;
        m_active_scenario_hash = hash;
        refreshRunModel();
        emit activeScenarioChanged();
    }

    void ScenarioBrowserViewModel::selectRun(const QString &hash, const double startTimeMs) {
        const domain::ScenarioRunId run_id{
            .scenario_id = domain::ScenarioId{
                .name = m_active_scenario_name.toStdString(),
                .hash = hash.toStdString(),
            },
            .start_time = static_cast<long long>(startTimeMs),
        };
        m_session_controller->setCurrentPerf(run_id);
    }

    void ScenarioBrowserViewModel::refreshRunModel() {
        if (m_active_scenario_hash.isEmpty()) {
            m_run_model->setRuns({});
            return;
        }

        const domain::ScenarioId scenario_id{
            .name = m_active_scenario_name.toStdString(),
            .hash = m_active_scenario_hash.toStdString(),
        };
        m_run_model->setRuns(m_session_controller->getRunsForScenario(scenario_id));
    }
}
