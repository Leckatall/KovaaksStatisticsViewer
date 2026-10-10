//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_AXIS_RENDERER_H
#define KOVAAKSSTATSVIEWER_AXIS_RENDERER_H

#include <QColor>
#include <QFont>
#include <QList>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include <functional>

class QPainter;

namespace ksv::ui {
    class AxisPainter {
    public:
        enum class Orientation { Horizontal, Vertical };

        struct Style {
            QColor gridColor{"#3D3D3D"};
            QColor textColor = QColor(255, 255, 255, 255*0.6);
            // When valid, the axis line itself (left edge for Vertical) is drawn in this colour.
            QColor axisLineColor;
            int fontPointSize = 8;
            qreal labelGap = 3;
            // Minimum free space between two neighbouring labels.
            qreal labelSpacing = 6;
        };

        // Where one tick's label is drawn.
        struct LabelPlacement {
            int tickIndex;
            QRectF rect;
            QString text;
        };

        // Lays out the tick labels so that none is clipped and none overlaps its neighbour. Each
        // label is measured and centred on its tick, then shifted inwards to stay inside `bounds`
        // (the whole canvas). When neighbouring labels would collide, only every n-th tick is
        // labelled, with n the smallest stride that keeps them apart; grid lines are unaffected.
        [[nodiscard]] static QList<LabelPlacement> layoutLabels(const QRectF &plot, const QRectF &bounds,
                                                                Orientation orientation, qreal boundMin,
                                                                qreal boundMax, const QList<qreal> &ticks,
                                                                const std::function<QString(qreal)> &formatTick,
                                                                const Style &style);

        static void paint(QPainter &painter, const QRectF &plot, const QRectF &bounds, Orientation orientation,
                          qreal boundMin, qreal boundMax, const QList<qreal> &ticks,
                          const std::function<QString(qreal)> &formatTick, const Style &style);
        static void paint(QPainter &painter, const QRectF &plot, const QRectF &bounds, Orientation orientation,
                          qreal boundMin, qreal boundMax, const QList<qreal> &ticks,
                          const std::function<QString(qreal)> &formatTick) {
            paint(painter, plot, bounds, orientation, boundMin, boundMax, ticks, formatTick, Style{});
        }

        // Pixel extent needed to draw `ticks` without clipping: max label width for Vertical
        // (reserved on the left), label height for Horizontal (reserved on the bottom).
        // Uses Style's own font/gap so this can never drift out of sync with paint().
        static qreal measureLabelExtent(Orientation orientation, const QList<qreal> &ticks,
                                        const std::function<QString(qreal)> &formatTick,
                                        const Style &style);
        static qreal measureLabelExtent(Orientation orientation, const QList<qreal> &ticks,
                                        const std::function<QString(qreal)> &formatTick) {
            return measureLabelExtent(orientation, ticks, formatTick, Style{});
        }

        // `accent` lightened just enough to read as small text on `background` (WCAG contrast of at
        // least `minimumContrast`); returned unchanged when it already does.
        [[nodiscard]] static QColor legibleOn(const QColor &accent, const QColor &background,
                                              qreal minimumContrast = 4.5);

    private:
        [[nodiscard]] static QFont tickFont(const Style &style);
    };
}

#endif //KOVAAKSSTATSVIEWER_AXIS_RENDERER_H
