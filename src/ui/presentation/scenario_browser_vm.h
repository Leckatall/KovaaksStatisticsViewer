//
// Created by Lecka on 09/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SCENARIO_BROWSER_VM_H
#define KOVAAKSSTATSVIEWER_SCENARIO_BROWSER_VM_H

#include <QObject>
#include <QString>
#include <memory>
#include <qqmlintegration.h>
#include <vector>

#include "scenario_list_model.h"
#include "run_list_model.h"
#include "app/usecases/i_session_controller.h"

namespace ksv::presentation {
    class ScenarioBrowserViewModel : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Created in C++")
        Q_PROPERTY(QObject* scenarioModel READ scenarioModel CONSTANT)
        Q_PROPERTY(QObject* runModel READ runModel CONSTANT)
        Q_PROPERTY(QObject* recentRunsModel READ recentRunsModel CONSTANT)
        Q_PROPERTY(QString activeScenarioHash READ activeScenarioHash NOTIFY activeScenarioChanged)
        Q_PROPERTY(QString currentRunHash READ currentRunHash NOTIFY currentRunChanged)
        Q_PROPERTY(double currentRunStartTimeMs READ currentRunStartTimeMs NOTIFY currentRunChanged)
        // Drives SelectionPanel's width. Taken from the unfiltered catalogue so that
        // filtering, sorting and run selection cannot resize the panel.
        Q_PROPERTY(QString longestScenarioName READ longestScenarioName NOTIFY longestScenarioNameChanged)

    public:
        enum RunSortField {
            Date = 0, Score, Accuracy, Duration
        };
        Q_ENUM(RunSortField)

        enum ScenarioSortField {
            LAST_PLAYED = 0, RUN_COUNT, NAME
        };
        Q_ENUM(ScenarioSortField)

        explicit ScenarioBrowserViewModel(std::shared_ptr<application::ISessionController> session_controller,
                                          QObject *parent = nullptr);

        [[nodiscard]] QObject *scenarioModel() const { return m_model; }
        [[nodiscard]] QObject *runModel() const { return m_run_model; }
        [[nodiscard]] QObject *recentRunsModel() const { return m_recent_runs_model; }
        [[nodiscard]] QString activeScenarioHash() const { return m_active_scenario_hash; }
        [[nodiscard]] QString currentRunHash() const { return m_current_run_hash; }
        [[nodiscard]] double currentRunStartTimeMs() const { return m_current_run_start_time_ms; }
        [[nodiscard]] QString longestScenarioName() const { return m_longest_scenario_name; }

        Q_INVOKABLE void setSearchText(const QString &text);
        Q_INVOKABLE void activateScenario(const QString &hash, const QString &name);
        Q_INVOKABLE void selectRun(const QString &hash, double startTimeMs);
        Q_INVOKABLE void setRunSort(RunSortField field, bool ascending);
        Q_INVOKABLE void setScenarioSort(ScenarioSortField field, bool ascending);
        Q_INVOKABLE void refresh();

    signals:
        void activeScenarioChanged();
        void currentRunChanged();
        void longestScenarioNameChanged();

    private:
        void applyFilter();
        void refreshScenarioModel();
        void refreshCurrentRun();
        void updateLongestScenarioName();
        void refreshRunModel();
        void applyRunSort(std::vector<application::RunSummary> &runs) const;
        void applyScenarioSort(std::vector<application::ScenarioSummary> &summaries) const;
        void refreshRecentRunsModel();

        static constexpr std::size_t kRecentRunsCount = 10;

        std::shared_ptr<application::ISessionController> m_session_controller;
        ScenarioListModel *m_model;
        RunListModel *m_run_model;
        RunListModel *m_recent_runs_model;
        std::vector<application::ScenarioSummary> m_all_summaries;
        QString m_search_text;
        QString m_active_scenario_hash;
        QString m_active_scenario_name;
        QString m_current_run_hash;
        double m_current_run_start_time_ms = 0;
        QString m_longest_scenario_name;
        RunSortField m_run_sort_field = RunSortField::Date;
        ScenarioSortField m_scenario_sort_field = ScenarioSortField::LAST_PLAYED;
        bool m_run_sort_ascending = false;
        bool m_scenario_sort_ascending = false;
    };
}

#endif //KOVAAKSSTATSVIEWER_SCENARIO_BROWSER_VM_H
