//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SERIES_PAINTER_H
#define KOVAAKSSTATSVIEWER_SERIES_PAINTER_H

#include <QColor>
#include <QPointF>
#include <QVector>
#include <QtGlobal>

#include <optional>

class QPainter;

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

        // Unmarked vertices the line also runs through, before the first and after the last point.
        struct Carry {
            std::optional<QPointF> leading;
            std::optional<QPointF> trailing;
        };

        // Draws the monotone curve through `pixelPoints` and, with showMarkers, a marker on each
        // point. A lone point without carry has no curve, so it is always drawn as a marker.
        static void paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const Carry &carry,
                          const QColor &lineColor, const Style &style);
        static void paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor,
                          const Style &style) {
            paint(painter, pixelPoints, Carry{}, lineColor, style);
        }
        static void paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor) {
            paint(painter, pixelPoints, lineColor, Style{});
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_SERIES_PAINTER_H
