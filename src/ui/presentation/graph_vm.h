//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_H

#include <QObject>
#include <QColor>
#include <QHash>
#include <QQmlListProperty>
#include <QVariantList>
#include <qqmlintegration.h>
#include <ranges>
#include <vector>

#include "axis_model.h"
#include "graph_vm_base.h"
#include "app/contracts/i_graph_use_case.h"

namespace ksv::presentation {
    class GraphViewModel : public GraphViewModelBase {
        Q_OBJECT
        Q_PROPERTY(QQmlListProperty<SeriesModel> allSeries READ allSeries NOTIFY seriesConfigurationChanged)
        Q_PROPERTY(QVariantList enabledSeriesIds READ enabledSeriesIds NOTIFY seriesConfigurationChanged)
        Q_PROPERTY(QString scenarioTitle READ scenarioTitle NOTIFY scenarioTitleChanged)

    public:
        explicit GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase, QObject *parent = nullptr);

        [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override;
        [[nodiscard]] AxisModel xAxis() const override { return m_timeAxis; }

        [[nodiscard]] QQmlListProperty<SeriesModel> allSeries() const {
            return QQmlListProperty<SeriesModel>(const_cast<GraphViewModel *>(this), &m_allSeriesList);
        }

        [[nodiscard]] QVariantList enabledSeriesIds() const { return m_enabledSeriesIds; }

        [[nodiscard]] QString scenarioTitle() const { return m_scenarioTitle; }

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
        // Fallback y-axis column when nothing else is specified; the built-in Score primitive, id 1.
        // DEPRECATED: Fallback uses every visible series before this default what is the point of having an axis for a graph with no data
        static constexpr int kDefaultYAxisSeriesId = 1;

        std::shared_ptr<application::IGraphUseCase> m_graphUseCase;
        AxisModel m_timeAxis{};
        QMap<QString, SeriesModel *> m_seriesById;
        QHash<uint64_t, application::AxisConfig> m_axesById;
        // Backs the allSeries QQmlListProperty; rebuilt (not just re-sorted) by fetchMetadata()/
        // fetchData() to match resolved.series' order — QMap<QString,...> iteration order (lexical
        // by id string) is not the display order QML expects.
        mutable QList<SeriesModel *> m_allSeriesList;
        QString m_scenarioTitle;
        QVariantList m_enabledSeriesIds;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_H
