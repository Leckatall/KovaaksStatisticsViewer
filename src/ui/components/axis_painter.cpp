//
// Created by Lecka on 08/08/2026.
//

#include "axis_painter.h"

#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QString>

namespace ksv::presentation {
    void AxisPainter::paint(QPainter &painter, const QRectF &plot, const Orientation orientation,
                             const qreal boundMin, const qreal boundMax, const QList<qreal> &ticks,
                             const std::function<QString(qreal)> &formatTick, const Style &style) {
        QFont tickFont = painter.font();
        tickFont.setPointSize(style.fontPointSize);
        painter.setFont(tickFont);

        const qreal span = boundMax - boundMin;

        for (const qreal value: ticks) {
            // t is [0,1] position on axis; zero span pins everything to mid-axis
            const qreal t = span != 0.0 ? (value - boundMin) / span : 0.5;

            painter.setPen(QPen(style.gridColor, 1));
            if (orientation == Orientation::Vertical) {
                const qreal y = plot.bottom() - t * plot.height();
                painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

                painter.setPen(style.textColor);
                painter.drawText(QRectF(0, y - 8, plot.left() - style.labelGap - 2, 16),
                                 Qt::AlignRight | Qt::AlignVCenter, formatTick(value));
            } else {
                const qreal x = plot.left() + t * plot.width();
                painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));

                painter.setPen(style.textColor);
                painter.drawText(QRectF(x - 20, plot.bottom() + style.labelGap, 40, 16),
                                 Qt::AlignHCenter | Qt::AlignTop, formatTick(value));
            }
        }
    }
}
