//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_AXIS_RENDERER_H
#define KOVAAKSSTATSVIEWER_AXIS_RENDERER_H

#include <QColor>
#include <QList>
#include <QRectF>
#include <QtGlobal>
#include <functional>

class QPainter;
class QString;

namespace ksv::presentation {
    class AxisPainter {
    public:
        enum class Orientation { Horizontal, Vertical };

        struct Style {
            QColor gridColor{"#2A2A2A"};
            QColor textColor{"white"};
            int fontPointSize = 8;
            qreal labelGap = 4;
        };

        static void paint(QPainter &painter, const QRectF &plot, Orientation orientation,
                          qreal boundMin, qreal boundMax, const QList<qreal> &ticks,
                          const std::function<QString(qreal)> &formatTick, const Style &style);
        static void paint(QPainter &painter, const QRectF &plot, Orientation orientation,
                          qreal boundMin, qreal boundMax, const QList<qreal> &ticks,
                          const std::function<QString(qreal)> &formatTick) {
            paint(painter, plot, orientation, boundMin, boundMax, ticks, formatTick, Style{});
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_AXIS_RENDERER_H
