#ifndef KOVAAKSSTATSVIEWER_GRAPH_SCENE_PAINTER_H
#define KOVAAKSSTATSVIEWER_GRAPH_SCENE_PAINTER_H

#include <QRectF>
#include <QtGlobal>

#include <optional>

#include "axis_painter.h"
#include "cancellation_check.h"
#include "graph_scene.h"

class QPainter;

namespace ksv::ui {
    // Pen width of a series' reference lines; a little heavier than the 1px gridlines.
    constexpr qreal kGraphReferenceLineWidth = 2.0;

    // Markers are drawn when consecutive points are at least this many pixels apart on average.
    constexpr qreal kGraphMinMarkerSpacing = 10.0;

    [[nodiscard]] bool graphMarkersFit(int pointCount, qreal pixelSpan);

    void paintGraphScene(QPainter &painter, const GraphScene &scene, const QRectF &bounds);

    // Once `cancelled` is set, painting stops before the next series and the picture is incomplete.
    void paintGraphScene(QPainter &painter, const GraphScene &scene, const QRectF &bounds,
                         const AxisPainter::Style &style, const CancellationCheck &cancelled = {});

    [[nodiscard]] QRectF graphPlotRect(const QRectF &bounds, const std::optional<GraphSceneAxis> &yAxis,
                                       const std::optional<GraphSceneAxis> &xAxis);

    [[nodiscard]] QRectF graphPlotRect(const QRectF &bounds, const std::optional<GraphSceneAxis> &yAxis,
                                       const std::optional<GraphSceneAxis> &xAxis, const AxisPainter::Style &style);
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_SCENE_PAINTER_H
