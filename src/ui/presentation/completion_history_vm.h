#ifndef KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_VM_H
#define KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_VM_H

#include <QList>
#include <QPointF>
#include <QQmlListProperty>
#include <QString>

#include <array>
#include <memory>
#include <vector>

#include "app/contracts/i_completion_history_use_case.h"
#include "value_axis.h"
#include "graph_vm_base.h"

namespace ksv::presentation {
    class CompletionHistoryViewModel final : public GraphViewModelBase {
        Q_OBJECT
        Q_PROPERTY(QQmlListProperty<SeriesModel> allSeries READ allSeries CONSTANT)
        Q_PROPERTY(QString scenarioTitle READ scenarioTitle NOTIFY scenarioTitleChanged)
        // Runs in the scenario's whole history, whatever the range shows.
        Q_PROPERTY(int runCount READ runCount NOTIFY dataUpdated)
        // Runs the committed range shows; 0 with a nonzero runCount means the range is empty.
        Q_PROPERTY(int visibleRunCount READ visibleRunCount NOTIFY dataUpdated)
        // The selected run's 1-based run number in the whole history, or -1 when no run is selected.
        // It stays set when the range hides the run.
        Q_PROPERTY(int selectedRunIndex READ selectedRunIndex NOTIFY selectedRunIndexChanged)
        // Whether the selected run is among the visible rows, i.e. whether the graph can mark it.
        Q_PROPERTY(bool selectedRunVisible READ selectedRunVisible NOTIFY selectedRunVisibleChanged)

    public:
        enum Column { RunIndex = 0, Score, Accuracy, Shots, Hits, Misses, ColumnCount };
        Q_ENUM(Column)

        explicit CompletionHistoryViewModel(
            std::shared_ptr<application::ICompletionHistoryUseCase> use_case,
            QObject *parent = nullptr);

        [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override;
        [[nodiscard]] const Axis &xAxis() const override { return m_xAxis; }
        [[nodiscard]] int yAxisColumn() const override { return -1; }
        [[nodiscard]] QString scenarioTitle() const { return m_scenario_title; }
        [[nodiscard]] int runCount() const { return m_run_count; }
        [[nodiscard]] int visibleRunCount() const { return static_cast<int>(m_visible_runs.size()); }
        [[nodiscard]] int selectedRunIndex() const { return m_selected_run_index; }
        [[nodiscard]] bool selectedRunVisible() const { return m_selected_run_visible; }

        // Selects the run with this 1-based run number (the history graph's x); a run the range
        // hides is ignored.
        Q_INVOKABLE void selectRun(int runIndex);

        // series objects for Score..Misses; identity is fixed at construction, only points/yAxis
        // mutate per refresh().
        [[nodiscard]] QQmlListProperty<SeriesModel> allSeries() const {
            return QQmlListProperty<SeriesModel>(const_cast<CompletionHistoryViewModel *>(this),
                                                  const_cast<QList<SeriesModel *> *>(&m_series));
        }

    public slots:
        void refresh();
        // Re-reads only the selected run; refresh() also does.
        void refreshSelection();

    signals:
        void scenarioTitleChanged();
        void selectedRunIndexChanged();
        void selectedRunVisibleChanged();

    private:
        std::shared_ptr<application::ICompletionHistoryUseCase> m_use_case;
        std::array<QList<QPointF>, ColumnCount> m_points{};
        ValueAxis m_xAxis;
        QList<SeriesModel *> m_series;
        QString m_scenario_title;
        int m_run_count = 0;
        // Ascending original run numbers of the visible rows.
        std::vector<int> m_visible_runs;
        int m_selected_run_index = -1;
        bool m_selected_run_visible = false;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_VM_H
