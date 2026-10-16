//
// Created by Lecka on 08/08/2026.
//

#include "series_painter.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>

#include "presentation/monotone_spline.h"

namespace ksv::ui {
    namespace {
        void drawMarkers(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor,
                         const SeriesPainter::Style &style) {
            const QColor fill = style.markerColor.isValid() ? style.markerColor : lineColor;
            if (style.markerOutlineColor.isValid())
                painter.setPen(QPen(style.markerOutlineColor, 1.5));
            else
                painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            for (const auto &p: pixelPoints) {
                if (!style.clip.isNull() && !style.clip.contains(p)) continue;
                painter.drawEllipse(p, style.markerRadius, style.markerRadius);
            }
        }

        void drawConnection(QPainter &painter, const QVector<QPointF> &vertices, const QColor &lineColor,
                            const SeriesPainter::Style &style) {
            const QVector<QPointF> curve = style.connection == SeriesPainter::Connection::Smooth
                                               ? presentation::monotoneCubicInterpolate(vertices, 16)
                                               : vertices;
            QPainterPath path;
            path.moveTo(curve.first());
            for (int p = 1; p < curve.size(); ++p) path.lineTo(curve[p]);

            painter.save();
            if (!style.clip.isNull()) painter.setClipRect(style.clip);
            painter.setPen(QPen(lineColor, style.lineWidth));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
            painter.restore();
        }
    }

    void SeriesPainter::paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const Carry &carry,
                              const QColor &lineColor, const Style &style) {
        const bool connected = style.connection != Connection::None;
        if (connected) {
            QVector<QPointF> vertices;
            vertices.reserve(pixelPoints.size() + 2);
            if (carry.leading) vertices.append(*carry.leading);
            vertices.append(pixelPoints);
            if (carry.trailing) vertices.append(*carry.trailing);
            if (vertices.size() >= 2) drawConnection(painter, vertices, lineColor, style);
        }

        const bool marked = !connected || style.showMarkers || pixelPoints.size() == 1;
        if (marked && !pixelPoints.isEmpty()) drawMarkers(painter, pixelPoints, lineColor, style);
    }
}
