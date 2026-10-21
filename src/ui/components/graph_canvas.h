//
// Created by Lecka on 07/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_CANVAS_H
#define KOVAAKSSTATSVIEWER_GRAPH_CANVAS_H

#include <QColor>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <limits>
#include <optional>
#include <qqmlintegration.h>

#include "presentation/graph_vm_base.h"
#include "series_painter.h"
#include "presentation/series_model.h"

namespace ksv::ui {
    class GraphCanvas : public QQuickPaintedItem {
        Q_OBJECT
        QML_ELEMENT
        Q_PROPERTY(presentation::GraphViewModelBase *graphVm READ graphVm WRITE setGraphVm NOTIFY graphVmChanged)
        Q_PROPERTY(QVariantList visibleColumns READ visibleColumns WRITE setVisibleColumns NOTIFY visibleColumnsChanged)
        Q_PROPERTY(QRectF plotArea READ plotRect NOTIFY plotAreaChanged)
        Q_PROPERTY(int yAxisColumn READ yAxisColumn WRITE setYAxisColumn NOTIFY yAxisColumnChanged)
        Q_PROPERTY(int labelledYAxisColumn READ labelledYAxisColumn NOTIFY labelledYAxisColumnChanged)
        // Every visible column whose line is measured on the labelled Y axis: the labelled series
        // plus any series sharing its axis (same yAxisId).
        Q_PROPERTY(QVariantList labelledYAxisColumns READ labelledYAxisColumns NOTIFY labelledYAxisColumnChanged)
        // The labelled axis' own name; a series with no shared axis titles it with its own name.
        Q_PROPERTY(QString labelledYAxisTitle READ labelledYAxisTitle NOTIFY labelledYAxisColumnChanged)
        // Mark each real data point with a dot whenever the points are far enough apart to tell
        // apart (few points); dense series stay plain lines.
        Q_PROPERTY(bool pointMarkers READ pointMarkers WRITE setPointMarkers NOTIFY pointMarkersChanged)
        // Whether markers are drawn at the current size and data (pointMarkers and sparse enough).
        Q_PROPERTY(bool pointMarkersShown READ pointMarkersShown NOTIFY plotAreaChanged)
        // Colour the graph is drawn on; used for marker outlines.
        Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
        // Colour of a series with no colour of its own (a neutral line), and of its tooltip swatch.
        Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY foregroundColorChanged)
        // Selection uses graph data units; NaN marks nothing.
        Q_PROPERTY(qreal markedX READ markedX WRITE setMarkedX NOTIFY markedXChanged)
        Q_PROPERTY(QColor markerColor READ markerColor WRITE setMarkerColor NOTIFY markerColorChanged)
        // How consecutive observations are joined; the values match application::ConnectionMode.
        // PointsOnly marks every observation whatever pointMarkers says or how dense the series is,
        // and leaves pointMarkers (the preference of the line modes) untouched.
        Q_PROPERTY(ConnectionMode connectionMode READ connectionMode WRITE setConnectionMode NOTIFY connectionModeChanged)

    public:
        enum class ConnectionMode { Smooth, Straight, PointsOnly };
        Q_ENUM(ConnectionMode)

        explicit GraphCanvas(QQuickItem *parent = nullptr);

        [[nodiscard]] ConnectionMode connectionMode() const { return m_connectionMode; }
        void setConnectionMode(ConnectionMode mode);

        [[nodiscard]] presentation::GraphViewModelBase *graphVm() const { return m_graphVm; }

        void setGraphVm(presentation::GraphViewModelBase *graphVm);

        [[nodiscard]] QVariantList visibleColumns() const { return m_visibleColumns; }

        void setVisibleColumns(const QVariantList &visibleColumns);

        [[nodiscard]] int yAxisColumn() const { return m_yAxisColumn; }

        void setYAxisColumn(int yAxisColumn);

        // Resolves yAxisColumn against visibleColumns, falling back to the VM default.
        [[nodiscard]] int labelledYAxisColumn() const;

        [[nodiscard]] QVariantList labelledYAxisColumns() const;

        [[nodiscard]] QString labelledYAxisTitle() const;

        [[nodiscard]] bool pointMarkers() const { return m_pointMarkers; }
        void setPointMarkers(bool pointMarkers);
        [[nodiscard]] bool pointMarkersShown() const;

        [[nodiscard]] QColor backgroundColor() const { return m_backgroundColor; }
        void setBackgroundColor(const QColor &backgroundColor);

        [[nodiscard]] QColor foregroundColor() const { return m_foregroundColor; }
        void setForegroundColor(const QColor &foregroundColor);

        // The colour `series` is drawn in: its own, or foregroundColor when it has none.
        [[nodiscard]] QColor colorFor(const presentation::SeriesModel &series) const;

        // Pen width of a series' reference lines; a little heavier than the 1px gridlines.
        static constexpr qreal kReferenceLineWidth = 2.0;

        [[nodiscard]] qreal markedX() const { return m_markedX; }
        void setMarkedX(qreal markedX);
        [[nodiscard]] QColor markerColor() const { return m_markerColor; }
        void setMarkerColor(const QColor &markerColor);

        // Markers are drawn when consecutive points are at least this many pixels apart on average.
        static constexpr qreal kMinMarkerSpacing = 10.0;
        [[nodiscard]] static bool markersFit(int pointCount, qreal pixelSpan);

        // Visible columns in the order their lines (and markers) are painted: the VM's list order
        // reversed, so the first series in the user's line list is painted last, on top.
        [[nodiscard]] QList<int> paintOrder() const;

        void paint(QPainter *painter) override;

        Q_INVOKABLE [[nodiscard]] QVariantMap nearestPoint(qreal x, qreal y) const;

        Q_INVOKABLE [[nodiscard]] QVariantMap valuesAtX(qreal x) const;

    signals:
        void graphVmChanged();
        void visibleColumnsChanged();
        void plotAreaChanged();
        void yAxisColumnChanged();
        void labelledYAxisColumnChanged();
        void pointMarkersChanged();
        void backgroundColorChanged();
        void foregroundColorChanged();
        void markedXChanged();
        void markerColorChanged();
        void connectionModeChanged();

    private:
        [[nodiscard]] QRectF plotRect() const;

        [[nodiscard]] SeriesPainter::Connection painterConnection() const;

        [[nodiscard]] QList<int> visibleColumnIds() const;

        [[nodiscard]] QList<const presentation::SeriesModel *> seriesInPaintOrder() const;

        struct LabelledAxisGroup {
            const presentation::SeriesModel *labelled = nullptr;
            QList<const presentation::SeriesModel *> members;
        };

        [[nodiscard]] LabelledAxisGroup labelledAxisGroup() const;

        [[nodiscard]] const presentation::Axis &xAxisFor(const presentation::SeriesModel &series) const;
        [[nodiscard]] presentation::ValueAxis yAxisFor(const presentation::SeriesModel &series) const;

        // Y axis for the series whose ticks are drawn on the y axis, if any is currently labelled.
        [[nodiscard]] std::optional<presentation::ValueAxis> labelledYAxis() const;

        [[nodiscard]] static QPointF toPixel(const QPointF &displayPoint, const QRectF &rect,
                                             const presentation::Axis &xAxis, const presentation::Axis &yAxis);

        void drawAxes(QPainter *painter, const QRectF &rect) const;

        void drawSeries(QPainter *painter, const QRectF &rect) const;

        void drawReferenceLines(QPainter *painter, const QRectF &rect) const;

        void drawMarkedX(QPainter *painter, const QRectF &rect) const;

        presentation::GraphViewModelBase *m_graphVm = nullptr;
        QVariantList m_visibleColumns;
        int m_yAxisColumn = -1;
        ConnectionMode m_connectionMode = ConnectionMode::Smooth;
        bool m_pointMarkers = false;
        QColor m_backgroundColor{"#2d2d2d"};
        QColor m_foregroundColor{Qt::white};
        qreal m_markedX = std::numeric_limits<qreal>::quiet_NaN();
        QColor m_markerColor{"#42d9e8"};
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_CANVAS_H
