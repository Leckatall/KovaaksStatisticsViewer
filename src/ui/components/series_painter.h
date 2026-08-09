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

namespace ksv::presentation {
    class SeriesPainter {
    public:
        struct Style {
            qreal lineWidth = 3;
            qreal markerRadius = 2;
            QColor markerColor{"white"};
        };

        static void paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor,
                          const Style &style);
        static void paint(QPainter &painter, const QVector<QPointF> &pixelPoints, const QColor &lineColor) {
            paint(painter, pixelPoints, lineColor, Style{});
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_SERIES_PAINTER_H
