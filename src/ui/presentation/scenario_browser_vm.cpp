//
// Created by Lecka on 09/08/2026.
//

#include "scenario_browser_vm.h"

#include <algorithm>

namespace ksv::presentation {
    ScenarioBrowserViewModel::ScenarioBrowserViewModel(
        std::shared_ptr<application::ISessionController> session_controller,
        QObject *parent) : QObject(parent),
                           m_session_controller(std::move(session_controller)),
                           m_model(new ScenarioListModel(this)),
                           m_run_model(new RunListModel(this)),
                           m_recent_runs_model(new RunListModel(this)) {
        connect(m_session_controller.get(), &application::ISessionController::currentPerfChanged,
                this, &ScenarioBrowserViewModel::refresh);
        refresh();
    }

    void ScenarioBrowserViewModel::refresh() {
        refreshScenarioModel();
        refreshRunModel();
        refreshRecentRunsModel();
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

    void ScenarioBrowserViewModel::refreshScenarioModel() {
        auto summaries = m_session_controller->getScenarioSummaries();
        applyScenarioSort(summaries);
        m_all_summaries = summaries;
        applyFilter();
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
        auto runs = m_session_controller->getRunsForScenario(scenario_id);
        applyRunSort(runs);
        m_run_model->setRuns(std::move(runs));
    }

    void ScenarioBrowserViewModel::setRunSort(const RunSortField field, const bool ascending) {
        m_run_sort_field = field;
        m_run_sort_ascending = ascending;
        refreshRunModel();
    }

    void ScenarioBrowserViewModel::setScenarioSort(ScenarioSortField field, bool ascending) {
        m_scenario_sort_field = field;
        m_scenario_sort_ascending = ascending;
        refreshScenarioModel();
    }

    void ScenarioBrowserViewModel::applyRunSort(std::vector<application::RunSummary> &runs) const {
        const bool ascending = m_run_sort_ascending;
        switch (m_run_sort_field) {
            case RunSortField::Score:
                std::ranges::stable_sort(runs, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.score < b.score : a.score > b.score;
                });
                break;
            case RunSortField::Accuracy:
                std::ranges::stable_sort(runs, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.accuracy < b.accuracy : a.accuracy > b.accuracy;
                });
                break;
            case RunSortField::Duration:
                std::ranges::stable_sort(runs, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.duration_seconds < b.duration_seconds : a.duration_seconds > b.duration_seconds;
                });
                break;
            case RunSortField::Date:
            default:
                std::ranges::stable_sort(runs, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.start_time_ms < b.start_time_ms : a.start_time_ms > b.start_time_ms;
                });
                break;
        }
    }

    void ScenarioBrowserViewModel::applyScenarioSort(std::vector<application::ScenarioSummary> &summaries) const {
        const bool ascending = m_scenario_sort_ascending;
        switch (m_scenario_sort_field) {
            case ScenarioSortField::RUN_COUNT:
                std::ranges::stable_sort(summaries, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.run_count < b.run_count : a.run_count > b.run_count;
                });
                break;
            case ScenarioSortField::LAST_PLAYED:
                std::ranges::stable_sort(summaries, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.last_played < b.last_played : a.last_played > b.last_played;
                });
                break;
            case ScenarioSortField::NAME:
                std::ranges::stable_sort(summaries, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.scenario_id.name < b.scenario_id.name : a.scenario_id.name > b.scenario_id.name;
                });
                break;
        }
    }

    void ScenarioBrowserViewModel::refreshRecentRunsModel() {
        m_recent_runs_model->setRuns(m_session_controller->getRecentRuns(kRecentRunsCount));
    }
}
