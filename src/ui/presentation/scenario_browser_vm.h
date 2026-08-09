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
        Q_PROPERTY(QString activeScenarioHash READ activeScenarioHash NOTIFY activeScenarioChanged)

    public:
        enum SortField {
            Date = 0, Score, Accuracy, Duration
        };
        Q_ENUM(SortField)

        explicit ScenarioBrowserViewModel(std::shared_ptr<application::ISessionController> session_controller,
                                          QObject *parent = nullptr);

        [[nodiscard]] QObject *scenarioModel() const { return m_model; }
        [[nodiscard]] QObject *runModel() const { return m_run_model; }
        [[nodiscard]] QString activeScenarioHash() const { return m_active_scenario_hash; }

        Q_INVOKABLE void setSearchText(const QString &text);
        Q_INVOKABLE void activateScenario(const QString &hash, const QString &name);
        Q_INVOKABLE void selectRun(const QString &hash, double startTimeMs);
        Q_INVOKABLE void setSort(SortField field, bool ascending);
        Q_INVOKABLE void refresh();

    signals:
        void activeScenarioChanged();

    private:
        void applyFilter();
        void refreshRunModel();
        void applySort(std::vector<application::RunSummary> &runs) const;

        std::shared_ptr<application::ISessionController> m_session_controller;
        ScenarioListModel *m_model;
        RunListModel *m_run_model;
        std::vector<application::ScenarioSummary> m_all_summaries;
        QString m_search_text;
        QString m_active_scenario_hash;
        QString m_active_scenario_name;
        SortField m_sort_field = SortField::Date;
        bool m_sort_ascending = false;
    };
}

#endif //KOVAAKSSTATSVIEWER_SCENARIO_BROWSER_VM_H
