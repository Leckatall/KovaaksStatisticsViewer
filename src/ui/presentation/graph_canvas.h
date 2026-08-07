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

#include "graph_vm.h"

namespace ksv::presentation {
    // Hand-rolled chart that replaces the QtGraphs-based DashboardGraph.qml
    // (which DashboardGraphCanvas.qml now supersedes in Main.qml). Exists
    // because QGraphsView reserves layout space per distinct axisY object
    // regardless of that axis's own visibility (a real Qt 6.11 limitation, not
    // something an axis subclass can override), and QSplineSeries has no
    // monotonicity control so it overshoots past the data's own value range
    // between points. This class owns its own axis/grid layout and uses a
    // monotone cubic Hermite interpolation (see monotone_spline.h) instead.
    class GraphCanvas : public QQuickPaintedItem {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(GraphViewModel *graphVm READ graphVm WRITE setGraphVm NOTIFY graphVmChanged)
        Q_PROPERTY(QVariantList visibleColumns READ visibleColumns WRITE setVisibleColumns NOTIFY visibleColumnsChanged)

    public:
        explicit GraphCanvas(QQuickItem *parent = nullptr);

        [[nodiscard]] GraphViewModel *graphVm() const { return m_graphVm; }
        void setGraphVm(GraphViewModel *graphVm);

        [[nodiscard]] QVariantList visibleColumns() const { return m_visibleColumns; }
        void setVisibleColumns(const QVariantList &visibleColumns);

        void paint(QPainter *painter) override;

        // Hit-tests (x, y) in item-local coordinates against the point
        // markers drawn during the last paint(). Returns
        // {valid, columnId, time, value}; `valid` is false when nothing is
        // within the hover radius.
        Q_INVOKABLE [[nodiscard]] QVariantMap nearestPoint(qreal x, qreal y) const;

    signals:
        void graphVmChanged();
        void visibleColumnsChanged();

    private:
        struct CachedSeries {
            int columnId = 0;
            QVector<QPointF> pixelPoints;
            QVector<QPointF> dataPoints; // {time, value}, matches pixelPoints index-for-index
        };

        [[nodiscard]] QRectF plotRect() const;
        [[nodiscard]] static QPointF toPixel(const QPointF &dataPoint, const QRectF &rect,
                                              const QPointF &xBounds, const QPointF &yBounds);

        void drawAxes(QPainter *painter, const QRectF &rect, const QVariantMap &bounds) const;
        void drawSeries(QPainter *painter, const QRectF &rect, const QVariantMap &bounds);

        GraphViewModel *m_graphVm = nullptr;
        QVariantList m_visibleColumns;
        QVector<CachedSeries> m_cachedSeries;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_CANVAS_H
