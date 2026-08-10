//
// Created by Lecka on 07/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_CANVAS_H
#define KOVAAKSSTATSVIEWER_GRAPH_CANVAS_H

#include <QQuickPaintedItem>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <qqmlintegration.h>

#include "presentation/graph_vm_base.h"
#include "presentation/series_model.h"

namespace ksv::ui {
    class GraphCanvas : public QQuickPaintedItem {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(presentation::GraphViewModelBase *graphVm READ graphVm WRITE setGraphVm NOTIFY graphVmChanged)
        Q_PROPERTY(QVariantList visibleColumns READ visibleColumns WRITE setVisibleColumns NOTIFY visibleColumnsChanged)
        Q_PROPERTY(QRectF plotArea READ plotRect NOTIFY plotAreaChanged)

    public:
        explicit GraphCanvas(QQuickItem *parent = nullptr);

        [[nodiscard]] presentation::GraphViewModelBase *graphVm() const { return m_graphVm; }

        void setGraphVm(presentation::GraphViewModelBase *graphVm);

        [[nodiscard]] QVariantList visibleColumns() const { return m_visibleColumns; }

        void setVisibleColumns(const QVariantList &visibleColumns);

        void paint(QPainter *painter) override;

        Q_INVOKABLE [[nodiscard]] QVariantMap nearestPoint(qreal x, qreal y) const;

        Q_INVOKABLE [[nodiscard]] QVariantMap valuesAtX(qreal x) const;

    signals:
        void graphVmChanged();
        void visibleColumnsChanged();
        void plotAreaChanged();

    private:
        [[nodiscard]] QRectF plotRect() const;

        [[nodiscard]] QList<int> visibleColumnIds() const;

        [[nodiscard]] presentation::AxisModel xAxisFor(const presentation::SeriesModel &series) const;
        [[nodiscard]] presentation::AxisModel yAxisFor(const presentation::SeriesModel &series) const;

        [[nodiscard]] static QPointF toPixel(const QPointF &displayPoint, const QRectF &rect,
                                             const presentation::AxisModel &xAxis, const presentation::AxisModel &yAxis);

        void drawAxes(QPainter *painter, const QRectF &rect) const;

        void drawSeries(QPainter *painter, const QRectF &rect) const;

        presentation::GraphViewModelBase *m_graphVm = nullptr;
        QVariantList m_visibleColumns;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_CANVAS_H
