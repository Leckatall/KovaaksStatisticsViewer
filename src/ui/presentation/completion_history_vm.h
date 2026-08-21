#ifndef KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_VM_H
#define KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_VM_H

#include <QList>
#include <QPointF>
#include <QQmlListProperty>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <array>
#include <memory>

#include "app/contracts/i_completion_history_use_case.h"
#include "axis_model.h"
#include "graph_vm_base.h"

namespace ksv::presentation {
    class CompletionHistoryViewModel final : public GraphViewModelBase {
        Q_OBJECT
        Q_PROPERTY(QQmlListProperty<SeriesModel> allSeries READ allSeries CONSTANT)
        Q_PROPERTY(QVariantList plottableColumns READ plottableColumns CONSTANT)
        Q_PROPERTY(QVariantMap axisBounds READ axisBounds NOTIFY boundsChanged)
        Q_PROPERTY(QString scenarioTitle READ scenarioTitle NOTIFY scenarioTitleChanged)
        Q_PROPERTY(int runCount READ runCount NOTIFY dataUpdated)

    public:
        enum Column { RunIndex = 0, Score, Accuracy, Shots, Hits, Misses, ColumnCount };
        Q_ENUM(Column)

        explicit CompletionHistoryViewModel(
            std::shared_ptr<application::ICompletionHistoryUseCase> use_case,
            QObject *parent = nullptr);

        [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override;
        [[nodiscard]] AxisModel xAxis() const override { return m_x_axis; }
        // TODO(2026-08-13): Remove with GraphViewModelBase::plottableColumns(); history QML uses the enum directly.
        [[nodiscard]] QVariantList plottableColumns() const override;
        [[nodiscard]] QVariantMap axisBounds() const override;
        [[nodiscard]] QList<qreal> axisTicks(int column) const override;
        [[nodiscard]] QList<QPointF> seriesPoints(int column) const override;
        [[nodiscard]] int xColumn() const override { return RunIndex; }
        [[nodiscard]] int yAxisColumn() const override { return -1; }
        [[nodiscard]] QString scenarioTitle() const { return m_scenario_title; }
        [[nodiscard]] int runCount() const { return m_run_count; }

        // series objects for Score..Misses; identity is fixed at construction, only points/yAxis
        // mutate per refresh().
        [[nodiscard]] QQmlListProperty<SeriesModel> allSeries() const {
            return QQmlListProperty<SeriesModel>(const_cast<CompletionHistoryViewModel *>(this),
                                                  const_cast<QList<SeriesModel *> *>(&m_series));
        }

    public slots:
        void refresh();

    signals:
        void scenarioTitleChanged();

    private:
        std::shared_ptr<application::ICompletionHistoryUseCase> m_use_case;
        std::array<QList<QPointF>, ColumnCount> m_points{};
        AxisModel m_x_axis;
        QList<SeriesModel *> m_series;
        QString m_scenario_title;
        int m_run_count = 0;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_VM_H
