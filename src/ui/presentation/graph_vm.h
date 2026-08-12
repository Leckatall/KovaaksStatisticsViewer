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
#include <array>
#include <qqmlintegration.h>
#include <ranges>

#include "axis_model.h"
#include "graph_vm_base.h"
#include "app/usecases/i_graph_use_case.h"

namespace ksv::presentation {
    class GraphViewModel : public GraphViewModelBase {
        Q_OBJECT
        Q_PROPERTY(QVariantList plottableColumns READ plottableColumns CONSTANT)
        Q_PROPERTY(QVariantMap axisBounds READ axisBounds NOTIFY boundsChanged)
        Q_PROPERTY(QString scenarioTitle READ scenarioTitle NOTIFY scenarioTitleChanged)

    public:
        enum Column {
            Time = 0, Score, Accuracy, Shots, Kills, Dmg, ScoreTotal, ExpectedFinalScore, ExpectedFinalScoreRecent,
            ColumnCount
        };
        Q_ENUM(Column)

        explicit GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase, QObject *parent = nullptr);

        void setData(QList<QMap<Column, qreal>> data);

        [[nodiscard]] QList<SeriesModel> series(const QList<int> &columns) const override {
            QList<SeriesModel> result;
            result.reserve(columns.size());
            for (const int c: columns) if (c >= Score && c < ColumnCount) result.append(m_series[c - Score]);
            return result;
        }
        [[nodiscard]] AxisModel xAxis() const override { return m_axes[Time]; }

        [[nodiscard]] QVariantList plottableColumns() const override;

        [[nodiscard]] QVariantMap axisBounds() const override;

        [[nodiscard]] QList<qreal> axisTicks(int column) const override;

        [[nodiscard]] QString scenarioTitle() const { return m_scenarioTitle; }

        Q_INVOKABLE [[nodiscard]] QString columnName(int column) const override;
        Q_INVOKABLE [[nodiscard]] QColor columnColor(int column) const override;
        Q_INVOKABLE [[nodiscard]] QString columnKey(int column) const override;

        [[nodiscard]] QList<QPointF> seriesPoints(int column) const override;

        [[nodiscard]] int xColumn() const override { return Time; }
        [[nodiscard]] int yAxisColumn() const override { return Score; }

        void recomputeBounds();

    public slots:
        void fetchData();
        void fetchData(const QString& scenario_id);
        void fetchLatestData();

    signals:
        void scenarioTitleChanged();

    private:
        std::shared_ptr<application::IGraphUseCase> m_graphUseCase;
        QList<QMap<Column, qreal>> m_data;
        std::array<AxisModel, ColumnCount> m_axes{};
        QList<SeriesModel> m_series;
        QString m_scenarioTitle;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_H
