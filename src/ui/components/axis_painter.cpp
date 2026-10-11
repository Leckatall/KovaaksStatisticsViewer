//
// Created by Lecka on 08/08/2026.
//

#include "axis_painter.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QString>
#include <algorithm>
#include <cmath>

namespace ksv::ui {
    namespace {
        // Free space kept between vertically stacked labels; horizontal labels use Style::labelSpacing.
        constexpr qreal kVerticalLabelSpacing = 2.0;

        qreal positionOnAxis(const qreal value, const qreal boundMin, const qreal boundMax) {
            // t is [0,1] position on axis; zero span pins everything to mid-axis
            const qreal span = boundMax - boundMin;
            return span != 0.0 ? (value - boundMin) / span : 0.5;
        }

        // Signed free space between two label rects along the axis direction (negative = overlap).
        qreal gapBetween(const QRectF &a, const QRectF &b, const AxisPainter::Orientation orientation) {
            if (orientation == AxisPainter::Orientation::Vertical)
                return std::max(a.top(), b.top()) - std::min(a.bottom(), b.bottom());
            return std::max(a.left(), b.left()) - std::min(a.right(), b.right());
        }
    }

    QFont AxisPainter::tickFont(const Style &style) {
        QFont font;
        font.setPointSize(style.fontPointSize);
        return font;
    }

    QList<AxisPainter::LabelPlacement> AxisPainter::layoutLabels(const QRectF &plot, const QRectF &bounds,
                                                                 const Orientation orientation,
                                                                 const qreal boundMin, const qreal boundMax,
                                                                 const QList<qreal> &ticks,
                                                                 const std::function<QString(qreal)> &formatTick,
                                                                 const Style &style) {
        QList<LabelPlacement> all;
        if (ticks.isEmpty()) return all;

        const QFontMetricsF metrics(tickFont(style));
        const qreal labelHeight = metrics.height();
        all.reserve(ticks.size());
        for (int i = 0; i < ticks.size(); ++i) {
            const qreal t = positionOnAxis(ticks[i], boundMin, boundMax);
            QString text = formatTick(ticks[i]);
            QRectF rect;
            if (orientation == Orientation::Vertical) {
                const qreal y = plot.bottom() - t * plot.height();
                const qreal right = plot.left() - style.labelGap - 2;
                rect = QRectF(bounds.left(), y - labelHeight / 2, std::max(0.0, right - bounds.left()), labelHeight);
                if (rect.top() < bounds.top()) rect.moveTop(bounds.top());
                if (rect.bottom() > bounds.bottom()) rect.moveBottom(bounds.bottom());
            } else {
                const qreal x = plot.left() + t * plot.width();
                const qreal labelWidth = std::ceil(metrics.horizontalAdvance(text)) + 1;
                rect = QRectF(x - labelWidth / 2, plot.bottom() + style.labelGap, labelWidth, labelHeight);
                if (rect.left() < bounds.left()) rect.moveLeft(bounds.left());
                if (rect.right() > bounds.right()) rect.moveRight(bounds.right());
            }
            all.append({i, rect, std::move(text)});
        }

        const qreal minimumGap = orientation == Orientation::Vertical ? kVerticalLabelSpacing : style.labelSpacing;
        const auto strideFits = [&](const int stride) {
            for (int i = stride; i < all.size(); i += stride)
                if (gapBetween(all[i - stride].rect, all[i].rect, orientation) < minimumGap) return false;
            return true;
        };
        int stride = 1;
        while (stride < all.size() && !strideFits(stride)) ++stride;
        if (stride == 1) return all;

        QList<LabelPlacement> kept;
        for (int i = 0; i < all.size(); i += stride) kept.append(all[i]);
        return kept;
    }

    void AxisPainter::paint(QPainter &painter, const QRectF &plot, const QRectF &bounds,
                            const Orientation orientation, const qreal boundMin, const qreal boundMax,
                            const QList<qreal> &ticks, const std::function<QString(qreal)> &formatTick,
                            const Style &style) {
        painter.save();
        painter.setFont(tickFont(style));

        painter.setPen(QPen(style.gridColor, 1));
        for (const qreal value: ticks) {
            const qreal t = positionOnAxis(value, boundMin, boundMax);
            if (orientation == Orientation::Vertical) {
                const qreal y = plot.bottom() - t * plot.height();
                painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            } else {
                const qreal x = plot.left() + t * plot.width();
                painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            }
        }

        painter.setPen(style.textColor);
        const Qt::Alignment alignment = orientation == Orientation::Vertical
                                            ? (Qt::AlignRight | Qt::AlignVCenter)
                                            : (Qt::AlignHCenter | Qt::AlignTop);
        for (const auto &label: layoutLabels(plot, bounds, orientation, boundMin, boundMax, ticks, formatTick, style))
            painter.drawText(label.rect, alignment, label.text);

        painter.restore();
    }

    qreal AxisPainter::measureLabelExtent(const Orientation orientation, const QList<qreal> &ticks,
                                          const std::function<QString(qreal)> &formatTick,
                                          const Style &style) {
        const QFontMetricsF metrics(tickFont(style));

        if (orientation == Orientation::Horizontal) return metrics.height();

        qreal maxWidth = 0.0;
        for (const qreal value: ticks) {
            maxWidth = std::max(maxWidth, metrics.horizontalAdvance(formatTick(value)));
        }
        return maxWidth;
    }
}
