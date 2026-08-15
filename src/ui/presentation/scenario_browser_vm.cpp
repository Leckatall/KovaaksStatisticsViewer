//
// Created by Lecka on 09/08/2026.
//

#include "scenario_browser_vm.h"

#include <algorithm>

namespace ksv::presentation {
    ScenarioBrowserViewModel::ScenarioBrowserViewModel(
        std::shared_ptr<application::IScenarioBrowserUseCase> scenario_browser_use_case,
        QObject *parent) : QObject(parent),
                            m_scenario_browser_use_case(std::move(scenario_browser_use_case)),
                           m_model(new ScenarioListModel(this)),
                           m_run_model(new RunListModel(this)),
                           m_recent_runs_model(new RunListModel(this)) {
        m_scenario_browser_use_case->onChanged(this, [this] { refresh(); });
        refresh();
    }

    void ScenarioBrowserViewModel::refresh() {
        refreshCurrentRun();
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
        auto summaries = m_scenario_browser_use_case->getScenarioSummaries();
        applyScenarioSort(summaries);
        m_all_summaries = summaries;
        updateLongestScenarioName();
        applyFilter();
    }

    void ScenarioBrowserViewModel::updateLongestScenarioName() {
        // Character count stands in for the pixel width; QML measures the winner once
        // with TextMetrics. Emitting only on a real change is what keeps the panel
        // width still — refreshScenarioModel() runs on every run selection.
        const auto longest = std::ranges::max_element(m_all_summaries, {}, [](const auto &summary) {
            return summary.scenario_id.name.size();
        });
        QString name = longest == m_all_summaries.end() ? QString() : QString::fromStdString(longest->scenario_id.name);
        if (name == m_longest_scenario_name) return;
        m_longest_scenario_name = std::move(name);
        emit longestScenarioNameChanged();
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
        m_scenario_browser_use_case->selectRun(run_id);
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
        auto runs = m_scenario_browser_use_case->getRunsForScenario(scenario_id);
        applyRunSort(runs);
        m_run_model->setRuns(std::move(runs));
    }

    void ScenarioBrowserViewModel::refreshCurrentRun() {
        const auto &run_id = m_scenario_browser_use_case->getCurrentPerf().run_id;
        const QString hash = QString::fromStdString(run_id.scenario_id.hash);
        const double start_time_ms = static_cast<double>(run_id.start_time);
        if (m_current_run_hash == hash && m_current_run_start_time_ms == start_time_ms)
            return;

        m_current_run_hash = hash;
        m_current_run_start_time_ms = start_time_ms;
        emit currentRunChanged();
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

    void ScenarioBrowserViewModel::applyRunSort(std::vector<domain::RunPerformance> &runs) const {
        const bool ascending = m_run_sort_ascending;
        switch (m_run_sort_field) {
            case RunSortField::Score:
                std::ranges::stable_sort(runs, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.completion.score < b.completion.score : a.completion.score > b.completion.score;
                });
                break;
            case RunSortField::Accuracy:
                std::ranges::stable_sort(runs, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.completion.accuracy() < b.completion.accuracy() : a.completion.accuracy() > b.completion.accuracy();
                });
                break;
            case RunSortField::Date:
            default:
                std::ranges::stable_sort(runs, [ascending](const auto &a, const auto &b) {
                    return ascending ? a.run_id.start_time < b.run_id.start_time : a.run_id.start_time > b.run_id.start_time;
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
        m_recent_runs_model->setRuns(m_scenario_browser_use_case->getRecentRuns(kRecentRunsCount));
    }
}
