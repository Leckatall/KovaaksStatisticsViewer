#include "graph_scene_painter.h"

#include <QPainter>
#include <QPen>
#include <QVector>
#include <algorithm>
#include <functional>

#include "axis_painter.h"
#include "series_painter.h"

namespace ksv::ui {
    namespace {
        constexpr qreal kMinLeftMargin = 20;
        constexpr qreal kMinBottomMargin = 16;
        constexpr qreal kTopMargin = 10;
        constexpr qreal kRightMargin = 20;

        std::function<QString(qreal)> labelsOf(const GraphSceneAxis &axis) {
            return [&axis](const qreal tick) { return axis.labelFor(tick); };
        }

        QPointF toPixel(const QPointF &displayPoint, const QRectF &rect,
                        const GraphSceneAxis &xAxis, const GraphSceneAxis &yAxis) {
            const qreal xt = xAxis.normalizedPosition(displayPoint.x());
            const qreal yt = yAxis.normalizedPosition(displayPoint.y());
            return {rect.left() + xt * rect.width(), rect.bottom() - yt * rect.height()};
        }

        // A canvas with no view model builds a scene whose x axis has no ticks, no labelled axis, no
        // series and no band, so every draw below is empty for it, as the live canvas requires.
        void drawAxes(QPainter &painter, const GraphScene &scene, const QRectF &rect, const QRectF &bounds,
                      const AxisPainter::Style &style) {
            // Only one series' Y axis gets labels; all project against their own axis.
            if (const auto &yAxis = scene.labelledYAxis) {
                AxisPainter::paint(painter, rect, bounds, AxisPainter::Orientation::Vertical,
                                   yAxis->min, yAxis->max, yAxis->ticks, labelsOf(*yAxis), style);
            }
            AxisPainter::paint(painter, rect, bounds, AxisPainter::Orientation::Horizontal,
                               scene.xAxis.min, scene.xAxis.max, scene.xAxis.ticks, labelsOf(scene.xAxis), style);
        }

        void drawReferenceLines(QPainter &painter, const GraphScene &scene, const QRectF &rect,
                                const CancellationCheck &cancelled) {
            painter.save();
            for (auto s = scene.series.crbegin(); s != scene.series.crend() && !isCancelled(cancelled); ++s) {
                for (const auto &line: s->references) {
                    const qreal t = s->yAxis.normalizedPosition(line.value);
                    if (t < 0.0 || t > 1.0) continue;
                    const qreal y = rect.bottom() - t * rect.height();
                    QPen pen(line.color, kGraphReferenceLineWidth);
                    pen.setCapStyle(Qt::FlatCap);
                    painter.setPen(pen);
                    painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
                }
            }
            painter.restore();
        }

        void drawSelectedBand(QPainter &painter, const GraphScene &scene, const QRectF &rect) {
            if (!scene.selectedBand) return;
            const qreal t = scene.xAxis.normalizedPosition(scene.selectedBand->x);
            if (t < 0.0 || t > 1.0) return;
            const qreal pixelX = rect.left() + t * rect.width();

            painter.save();
            painter.setClipRect(rect, Qt::IntersectClip);
            QColor bandColor = scene.selectedBand->color;
            bandColor.setAlphaF(bandColor.alphaF() * 0.25);
            painter.fillRect(QRectF(pixelX - 4.0, rect.top(), 8.0, rect.height()), bandColor);
            painter.fillRect(QRectF(pixelX - 1.0, rect.bottom() - 4.0, 2.0, 4.0), scene.selectedBand->color);
            painter.restore();
        }

        void drawSeries(QPainter &painter, const GraphScene &scene, const QRectF &rect,
                        const CancellationCheck &cancelled) {
            // Painted back to front, so the series highest in the line list ends up on top, with its
            // markers stacked the same way as its line.
            for (auto s = scene.series.crbegin(); s != scene.series.crend() && !isCancelled(cancelled); ++s) {
                if (s->observations.isEmpty() && !s->leadingCarry && !s->trailingCarry) continue;

                const auto carryPixel = [&](const std::optional<QPointF> &carry) -> std::optional<QPointF> {
                    if (!carry) return std::nullopt;
                    return toPixel(*carry, rect, s->xAxis, s->yAxis);
                };

                QVector<QPointF> pixelPoints;
                pixelPoints.reserve(s->observations.size());
                for (const auto &p: s->observations) pixelPoints.append(toPixel(p, rect, s->xAxis, s->yAxis));

                SeriesPainter::Style style;
                style.lineWidth = s->lineWidth;
                style.connection = scene.connection;
                // A line centred on the plot edge keeps its full stroke; anything further out is cut.
                const qreal edgeAllowance = s->lineWidth / 2;
                style.clip = rect.adjusted(-edgeAllowance, -edgeAllowance, edgeAllowance, edgeAllowance);
                style.markerOutlineColor = scene.background;
                style.showMarkers = scene.pointMarkers && !pixelPoints.isEmpty() &&
                                    graphMarkersFit(int(pixelPoints.size()),
                                                    pixelPoints.back().x() - pixelPoints.front().x());
                SeriesPainter::paint(painter, pixelPoints, {carryPixel(s->leadingCarry), carryPixel(s->trailingCarry)},
                                     s->color, style);
            }
        }
    }

    bool graphMarkersFit(const int pointCount, const qreal pixelSpan) {
        if (pointCount < 2) return pointCount == 1;
        return pixelSpan / (pointCount - 1) >= kGraphMinMarkerSpacing;
    }

    void paintGraphScene(QPainter &painter, const GraphScene &scene, const QRectF &bounds) {
        paintGraphScene(painter, scene, bounds, AxisPainter::Style{});
    }

    void paintGraphScene(QPainter &painter, const GraphScene &scene, const QRectF &bounds,
                         const AxisPainter::Style &style, const CancellationCheck &cancelled) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF rect = graphPlotRect(bounds, scene.labelledYAxis, scene.xAxis, style);
        drawAxes(painter, scene, rect, bounds, style);
        drawReferenceLines(painter, scene, rect, cancelled);
        drawSelectedBand(painter, scene, rect);
        drawSeries(painter, scene, rect, cancelled);
    }

    QRectF graphPlotRect(const QRectF &bounds, const std::optional<GraphSceneAxis> &yAxis,
                         const std::optional<GraphSceneAxis> &xAxis) {
        return graphPlotRect(bounds, yAxis, xAxis, AxisPainter::Style{});
    }

    QRectF graphPlotRect(const QRectF &bounds, const std::optional<GraphSceneAxis> &yAxis,
                         const std::optional<GraphSceneAxis> &xAxis, const AxisPainter::Style &style) {
        const qreal labelExtentPadding = style.labelGap + 2;
        qreal leftMargin = kMinLeftMargin;
        qreal bottomMargin = kMinBottomMargin;

        if (yAxis) {
            leftMargin = std::max(kMinLeftMargin,
                                  AxisPainter::measureLabelExtent(AxisPainter::Orientation::Vertical, yAxis->ticks,
                                                                  labelsOf(*yAxis), style) + labelExtentPadding);
        }
        if (xAxis) {
            bottomMargin = std::max(kMinBottomMargin,
                                    AxisPainter::measureLabelExtent(AxisPainter::Orientation::Horizontal, xAxis->ticks,
                                                                    labelsOf(*xAxis), style) + labelExtentPadding);
        }

        return {
            bounds.left() + leftMargin, bounds.top() + kTopMargin,
            qMax(0.0, bounds.width() - leftMargin - kRightMargin),
            qMax(0.0, bounds.height() - kTopMargin - bottomMargin)
        };
    }
}
