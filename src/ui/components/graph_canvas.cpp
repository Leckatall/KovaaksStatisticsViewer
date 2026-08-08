//
// Created by Lecka on 07/08/2026.
//

#include "graph_canvas.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <cmath>

#include "presentation/monotone_spline.h"

namespace ksv::presentation {
    namespace {
        constexpr qreal kLeftMargin = 55;
        constexpr qreal kBottomMargin = 28;
        constexpr qreal kTopMargin = 10;
        constexpr qreal kRightMargin = 10;
        constexpr int kGridLines = 5;
        constexpr qreal kMarkerRadius = 4;
        constexpr qreal kHoverRadius = 10;

        const QColor kGridColor("#2A2A2A");
        const QColor kTextColor("white");
        const QColor kMarkerColor("#4DD0E1");

        QString formatTick(const qreal value) {
            return QString::number(value, 'f', std::abs(value - std::round(value)) < 1e-6 ? 0 : 1);
        }
    }

    GraphCanvas::GraphCanvas(QQuickItem *parent) : QQuickPaintedItem(parent) {
        setAntialiasing(true);
        connect(this, &QQuickItem::widthChanged, this, &GraphCanvas::plotAreaChanged);
        connect(this, &QQuickItem::heightChanged, this, &GraphCanvas::plotAreaChanged);
    }

    void GraphCanvas::setGraphVm(GraphViewModel *graphVm) {
        if (m_graphVm == graphVm) return;
        if (m_graphVm) m_graphVm->disconnect(this);
        m_graphVm = graphVm;
        if (m_graphVm) {
            connect(m_graphVm, &GraphViewModel::pointCountChanged, this, [this] { update(); });
            connect(m_graphVm, &GraphViewModel::boundsChanged, this, [this] { update(); });
        }
        emit graphVmChanged();
        update();
    }

    void GraphCanvas::setVisibleColumns(const QVariantList &visibleColumns) {
        if (m_visibleColumns == visibleColumns) return;
        m_visibleColumns = visibleColumns;
        emit visibleColumnsChanged();
        update();
    }

    QRectF GraphCanvas::plotRect() const {
        return {kLeftMargin, kTopMargin,
                qMax(0.0, width() - kLeftMargin - kRightMargin),
                qMax(0.0, height() - kTopMargin - kBottomMargin)};
    }

    QPointF GraphCanvas::toPixel(const QPointF &dataPoint, const QRectF &rect,
                                  const QPointF &xBounds, const QPointF &yBounds) {
        const qreal xSpan = xBounds.y() - xBounds.x();
        const qreal ySpan = yBounds.y() - yBounds.x();
        const qreal xt = xSpan != 0.0 ? (dataPoint.x() - xBounds.x()) / xSpan : 0.5;
        const qreal yt = ySpan != 0.0 ? (dataPoint.y() - yBounds.x()) / ySpan : 0.5;
        return {rect.left() + xt * rect.width(), rect.bottom() - yt * rect.height()};
    }

    void GraphCanvas::drawAxes(QPainter *painter, const QRectF &rect, const QVariantMap &bounds) const {
        if (!m_graphVm) return;
        const QPointF timeBounds = bounds[QString::number(GraphViewModel::Time)].toPointF();
        const QPointF scoreBounds = bounds[QString::number(GraphViewModel::Score)].toPointF();

        QFont tickFont = painter->font();
        tickFont.setPointSize(8);
        painter->setFont(tickFont);

        for (int i = 0; i <= kGridLines; ++i) {
            const qreal t = qreal(i) / kGridLines;
            const qreal y = rect.bottom() - t * rect.height();

            painter->setPen(QPen(kGridColor, 1));
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));

            const qreal value = scoreBounds.x() + t * (scoreBounds.y() - scoreBounds.x());
            painter->setPen(kTextColor);
            painter->drawText(QRectF(0, y - 8, kLeftMargin - 6, 16), Qt::AlignRight | Qt::AlignVCenter, formatTick(value));
        }

        for (int i = 0; i <= kGridLines; ++i) {
            const qreal t = qreal(i) / kGridLines;
            const qreal x = rect.left() + t * rect.width();

            painter->setPen(QPen(kGridColor, 1));
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));

            const qreal value = timeBounds.x() + t * (timeBounds.y() - timeBounds.x());
            painter->setPen(kTextColor);
            painter->drawText(QRectF(x - 20, rect.bottom() + 4, 40, kBottomMargin - 4),
                               Qt::AlignHCenter | Qt::AlignTop, formatTick(value));
        }
    }

    void GraphCanvas::drawSeries(QPainter *painter, const QRectF &rect, const QVariantMap &bounds) {
        m_cachedSeries.clear();
        if (!m_graphVm || m_graphVm->pointCount() < 2) return;

        const QPointF timeBounds = bounds[QString::number(GraphViewModel::Time)].toPointF();
        const auto plottable = m_graphVm->plottableColumns();

        for (int i = 0; i < plottable.size(); ++i) {
            if (i >= m_visibleColumns.size() || !m_visibleColumns[i].toBool()) continue;

            const auto column = static_cast<GraphViewModel::Column>(plottable[i].toInt());
            const QPointF columnBounds = bounds[QString::number(column)].toPointF();

            QVector<QPointF> dataPoints = m_graphVm->seriesPoints(column);
            if (dataPoints.size() < 2) continue;

            QVector<QPointF> pixelPoints;
            pixelPoints.reserve(dataPoints.size());
            for (const auto &p: dataPoints) pixelPoints.append(toPixel(p, rect, timeBounds, columnBounds));

            const auto curve = monotoneCubicInterpolate(pixelPoints, 16);

            QPainterPath path;
            path.moveTo(curve.first());
            for (int p = 1; p < curve.size(); ++p) path.lineTo(curve[p]);

            painter->setPen(QPen(m_graphVm->columnColor(column), 3));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(path);

            painter->setPen(Qt::NoPen);
            painter->setBrush(kMarkerColor);
            for (const auto &p: pixelPoints) painter->drawEllipse(p, kMarkerRadius, kMarkerRadius);

            m_cachedSeries.append({int(column), std::move(pixelPoints), std::move(dataPoints)});
        }
    }

    void GraphCanvas::paint(QPainter *painter) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF rect = plotRect();
        const QVariantMap bounds = m_graphVm ? m_graphVm->axisBounds() : QVariantMap{};
        drawAxes(painter, rect, bounds);
        drawSeries(painter, rect, bounds);
    }

    QVariantMap GraphCanvas::valuesAtX(const qreal x) const {
        QVariantMap result;
        result["valid"] = false;
        if (m_cachedSeries.isEmpty() || !m_graphVm) return result;

        qreal bestDist = std::numeric_limits<qreal>::max();
        int bestIndex = -1;
        const auto &firstSeries = m_cachedSeries.first();
        for (int i = 0; i < firstSeries.pixelPoints.size(); ++i) {
            const qreal dist = std::abs(firstSeries.pixelPoints[i].x() - x);
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = i;
            }
        }
        if (bestIndex < 0) return result;

        result["valid"] = true;
        result["time"] = firstSeries.dataPoints[bestIndex].x();
        result["pixelX"] = firstSeries.pixelPoints[bestIndex].x();

        QVariantList seriesList;
        for (const auto &series : m_cachedSeries) {
            if (bestIndex >= series.dataPoints.size()) continue;
            QVariantMap entry;
            const auto column = static_cast<GraphViewModel::Column>(series.columnId);
            entry["name"] = m_graphVm->columnName(column);
            entry["color"] = m_graphVm->columnColor(column).name();
            entry["value"] = series.dataPoints[bestIndex].y();
            seriesList.append(entry);
        }
        result["series"] = seriesList;
        return result;
    }

    QVariantMap GraphCanvas::nearestPoint(const qreal x, const qreal y) const {
        qreal bestDistanceSq = kHoverRadius * kHoverRadius;
        QVariantMap best;
        best["valid"] = false;

        for (const auto &series: m_cachedSeries) {
            for (int i = 0; i < series.pixelPoints.size(); ++i) {
                const QPointF &p = series.pixelPoints[i];
                const qreal dx = p.x() - x;
                const qreal dy = p.y() - y;
                const qreal distSq = dx * dx + dy * dy;
                if (distSq <= bestDistanceSq) {
                    bestDistanceSq = distSq;
                    best["valid"] = true;
                    best["columnId"] = series.columnId;
                    best["time"] = series.dataPoints[i].x();
                    best["value"] = series.dataPoints[i].y();
                }
            }
        }
        return best;
    }
}
