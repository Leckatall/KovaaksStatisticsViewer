//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_H

#include <QObject>
#include <QColor>
#include <QPointF>
#include <QVariantList>
#include <QVariantMap>
#include <qqmlintegration.h>
#include <ranges>

#include "axis_model.h"
#include "graph_vm_base.h"
#include "app/contracts/i_graph_use_case.h"

namespace ksv::presentation {
    class GraphViewModel : public GraphViewModelBase {
        Q_OBJECT
        Q_PROPERTY(QVariantList allSeries READ allSeries NOTIFY seriesConfigurationChanged)
        Q_PROPERTY(QVariantList enabledSeriesIds READ enabledSeriesIds NOTIFY seriesConfigurationChanged)
        Q_PROPERTY(QVariantMap axisBounds READ axisBounds NOTIFY boundsChanged)
        Q_PROPERTY(QString scenarioTitle READ scenarioTitle NOTIFY scenarioTitleChanged)

    public:
        explicit GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase, QObject *parent = nullptr);

        void setData(QList<QMap<int, qreal>> data);

        [[nodiscard]] QList<SeriesModel> series(const QList<int> &columns) const override;
        [[nodiscard]] AxisModel xAxis() const override { return m_timeAxis; }

        [[nodiscard]] QVariantList allSeries() const { return m_allSeries; }
        [[nodiscard]] QVariantList enabledSeriesIds() const { return m_enabledSeriesIds; }

        // Unused by any real caller (GraphCanvas gets axis info via series()/xAxis()); kept minimal
        // only to satisfy GraphViewModelBase's pure-virtual contract.
        [[nodiscard]] QVariantMap axisBounds() const override;
        [[nodiscard]] QList<qreal> axisTicks(int column) const override;

        [[nodiscard]] QString scenarioTitle() const { return m_scenarioTitle; }

        Q_INVOKABLE [[nodiscard]] QString columnName(int column) const override;
        Q_INVOKABLE [[nodiscard]] QColor columnColor(int column) const override;
        Q_INVOKABLE [[nodiscard]] QString columnKey(int column) const override;

        [[nodiscard]] QList<QPointF> seriesPoints(int column) const override;

        [[nodiscard]] int xColumn() const override { return kTimeColumn; }
        [[nodiscard]] int yAxisColumn() const override { return kDefaultYAxisSeriesId; }

        void recomputeBounds();

        Q_INVOKABLE QString seriesIdForColumn(int column) const;
        Q_INVOKABLE [[nodiscard]] int columnForSeriesId(const QString &id) const;

    public slots:
        void fetchData();
        void fetchData(const QString& scenario_id);
        void fetchLatestData();
        void fetchMetadata();

    signals:
        void scenarioTitleChanged();
        void seriesConfigurationChanged();

    private:
        // `column` is always a series' own SeriesId::value, never a position — see graph_vm.cpp.
        // Time has no SeriesId (primitives start at 1), so 0 is reserved for it below.
        static constexpr int kTimeColumn = 0;
        // Fallback y-axis column when nothing else is specified; the built-in Score primitive, id 1.
        static constexpr int kDefaultYAxisSeriesId = 1;

        std::shared_ptr<application::IGraphUseCase> m_graphUseCase;
        QList<QMap<int, qreal>> m_data;
        AxisModel m_timeAxis{};
        QMap<QString, SeriesModel> m_seriesById;
        QString m_scenarioTitle;
        QVariantList m_allSeries;
        QVariantList m_enabledSeriesIds;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_H
