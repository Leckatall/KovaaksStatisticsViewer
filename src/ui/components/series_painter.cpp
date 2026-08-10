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
    void SeriesPainter::paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor,
                              const Style &style) {
        if (pixelPoints.size() < 2) return;

        const auto curve = presentation::monotoneCubicInterpolate(pixelPoints, 16);

        QPainterPath path;
        path.moveTo(curve.first());
        for (int p = 1; p < curve.size(); ++p) path.lineTo(curve[p]);

        painter.setPen(QPen(lineColor, style.lineWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        painter.setPen(Qt::NoPen);
        painter.setBrush(style.markerColor);
        for (const auto &p: pixelPoints) painter.drawEllipse(p, style.markerRadius, style.markerRadius);
    }
}
