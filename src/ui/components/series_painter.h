//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SERIES_PAINTER_H
#define KOVAAKSSTATSVIEWER_SERIES_PAINTER_H

#include <QColor>
#include <QVector>
#include <QtGlobal>

class QPainter;
class QPointF;

namespace ksv::ui {
    class SeriesPainter {
    public:
        struct Style {
            qreal lineWidth = 3;
            // Data-point markers: a dot on each real sample so it reads apart from the interpolated
            // curve. An invalid markerColor means the line colour; an invalid outline draws none.
            qreal markerRadius = 3.5;
            QColor markerColor;
            QColor markerOutlineColor;
            bool showMarkers = false;
        };

        // Draws the monotone curve through `pixelPoints` and, with showMarkers, a marker on each
        // point. A lone point has no curve, so it is always drawn as a marker.
        static void paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor,
                          const Style &style);
        static void paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor) {
            paint(painter, pixelPoints, lineColor, Style{});
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_SERIES_PAINTER_H
