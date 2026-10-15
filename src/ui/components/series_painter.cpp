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
            for (const auto &p: pixelPoints) painter.drawEllipse(p, style.markerRadius, style.markerRadius);
        }
    }

    void SeriesPainter::paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor,
                              const Style &style) {
        if (pixelPoints.isEmpty()) return;
        if (pixelPoints.size() == 1) {
            drawMarkers(painter, pixelPoints, lineColor, style);
            return;
        }

        const auto curve = presentation::monotoneCubicInterpolate(pixelPoints, 16);

        QPainterPath path;
        path.moveTo(curve.first());
        for (int p = 1; p < curve.size(); ++p) path.lineTo(curve[p]);

        painter.setPen(QPen(lineColor, style.lineWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        if (style.showMarkers) drawMarkers(painter, pixelPoints, lineColor, style);
    }
}
