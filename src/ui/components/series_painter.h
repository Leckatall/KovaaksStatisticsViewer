//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SERIES_PAINTER_H
#define KOVAAKSSTATSVIEWER_SERIES_PAINTER_H

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QtGlobal>

#include <optional>

class QPainter;

namespace ksv::ui {
    class SeriesPainter {
    public:
        // None draws only the markers of the observations, whatever showMarkers says.
        enum class Connection { Smooth, Straight, None };

        struct Style {
            qreal lineWidth = 3;
            Connection connection = Connection::Smooth;
            // A non-null rect confines the line and drops markers centred outside it.
            QRectF clip;
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

        // Draws the connection through `pixelPoints` (carry included) and, with showMarkers, a marker
        // on each point. A lone point is always marked.
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
