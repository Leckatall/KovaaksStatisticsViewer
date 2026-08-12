//
// Created by Lecka on 07/08/2026.
//

#include "graph_canvas.h"

#include <QColor>
#include <QPainter>
#include <algorithm>
#include <limits>

#include "axis_painter.h"
#include "series_painter.h"

namespace ksv::ui {
    namespace {
        constexpr qreal kMinLeftMargin = 20;
        constexpr qreal kMinBottomMargin = 16;
        constexpr qreal kTopMargin = 10;
        constexpr qreal kRightMargin = 10;
        constexpr qreal kHoverRadius = 10;
        // Matches the fudge AxisPainter::paint already bakes into its label rects.
        const qreal kLabelExtentPadding = AxisPainter::Style{}.labelGap + 2;
    }

    GraphCanvas::GraphCanvas(QQuickItem *parent) : QQuickPaintedItem(parent) {
        setAntialiasing(true);
        connect(this, &QQuickItem::widthChanged, this, &GraphCanvas::plotAreaChanged);
        connect(this, &QQuickItem::heightChanged, this, &GraphCanvas::plotAreaChanged);
    }

    void GraphCanvas::setGraphVm(presentation::GraphViewModelBase *graphVm) {
        if (m_graphVm == graphVm) return;
        if (m_graphVm) m_graphVm->disconnect(this);
        m_graphVm = graphVm;
        if (m_graphVm) {
            connect(m_graphVm, &presentation::GraphViewModelBase::dataUpdated, this, [this] {
                emit plotAreaChanged();
                update();
            });
            connect(m_graphVm, &presentation::GraphViewModelBase::boundsChanged, this, [this] {
                emit plotAreaChanged();
                update();
            });
        }
        emit graphVmChanged();
        emit labelledYAxisColumnChanged();
        emit plotAreaChanged();
        update();
    }

    void GraphCanvas::setVisibleColumns(const QVariantList &visibleColumns) {
        if (m_visibleColumns == visibleColumns) return;
        m_visibleColumns = visibleColumns;
        emit visibleColumnsChanged();
        emit labelledYAxisColumnChanged();
        emit plotAreaChanged();
        update();
    }

    void GraphCanvas::setYAxisColumn(const int yAxisColumn) {
        if (m_yAxisColumn == yAxisColumn) return;
        m_yAxisColumn = yAxisColumn;
        emit yAxisColumnChanged();
        emit labelledYAxisColumnChanged();
        emit plotAreaChanged();
        update();
    }

    int GraphCanvas::labelledYAxisColumn() const {
        const QList<int> visible = visibleColumnIds();
        if (visible.contains(m_yAxisColumn)) return m_yAxisColumn;
        if (!visible.isEmpty()) return visible.front();
        return m_graphVm ? m_graphVm->yAxisColumn() : -1;
    }

    QList<int> GraphCanvas::visibleColumnIds() const {
        QList<int> ids;
        ids.reserve(m_visibleColumns.size());
        for (const auto &v: m_visibleColumns) ids.append(v.toInt());
        return ids;
    }

    std::optional<presentation::AxisModel> GraphCanvas::labelledYAxis() const {
        if (!m_graphVm) return std::nullopt;
        const auto labelled = m_graphVm->series({labelledYAxisColumn()});
        if (labelled.isEmpty()) return std::nullopt;
        return yAxisFor(labelled.front());
    }

    QRectF GraphCanvas::plotRect() const {
        qreal leftMargin = kMinLeftMargin;
        qreal bottomMargin = kMinBottomMargin;

        if (m_graphVm) {
            if (const auto yAxis = labelledYAxis()) {
                leftMargin = std::max(kMinLeftMargin,
                    AxisPainter::measureLabelExtent(AxisPainter::Orientation::Vertical, yAxis->ticks(),
                        [&yAxis](const qreal v) { return yAxis->formatTick(v); }) + kLabelExtentPadding);
            }
            const presentation::AxisModel xAxis = m_graphVm->xAxis();
            bottomMargin = std::max(kMinBottomMargin,
                AxisPainter::measureLabelExtent(AxisPainter::Orientation::Horizontal, xAxis.ticks(),
                    [&xAxis](const qreal v) { return xAxis.formatTick(v); }) + kLabelExtentPadding);
        }

        return {leftMargin, kTopMargin,
                qMax(0.0, width() - leftMargin - kRightMargin),
                qMax(0.0, height() - kTopMargin - bottomMargin)};
    }

    presentation::AxisModel GraphCanvas::xAxisFor(const presentation::SeriesModel &series) const {
        return series.xAxis.value_or(m_graphVm->xAxis());
    }

    presentation::AxisModel GraphCanvas::yAxisFor(const presentation::SeriesModel &series) const {
        return series.yAxis.value_or(series.deriveYAxis());
    }

    QPointF GraphCanvas::toPixel(const QPointF &displayPoint, const QRectF &rect,
                                  const presentation::AxisModel &xAxis, const presentation::AxisModel &yAxis) {
        const qreal xt = xAxis.normalizedPosition(displayPoint.x());
        const qreal yt = yAxis.normalizedPosition(displayPoint.y());
        return {rect.left() + xt * rect.width(), rect.bottom() - yt * rect.height()};
    }

    void GraphCanvas::drawAxes(QPainter *painter, const QRectF &rect) const {
        if (!m_graphVm) return;
        const presentation::AxisModel xAxis = m_graphVm->xAxis();

        // Only one series' Y axis gets labels; all project against their own axis
        if (const auto yAxis = labelledYAxis()) {
            AxisPainter::paint(*painter, rect, AxisPainter::Orientation::Vertical,
                                yAxis->min(), yAxis->max(), yAxis->ticks(),
                                [&yAxis](const qreal v) { return yAxis->formatTick(v); });
        }
        AxisPainter::paint(*painter, rect, AxisPainter::Orientation::Horizontal,
                            xAxis.min(), xAxis.max(), xAxis.ticks(),
                            [&xAxis](const qreal v) { return xAxis.formatTick(v); });
    }

    void GraphCanvas::drawSeries(QPainter *painter, const QRectF &rect) const {
        if (!m_graphVm) return;

        for (const auto series = m_graphVm->series(visibleColumnIds());
            const auto &s: series) {
            const QList<QPointF> displayPoints = s.displayPoints();
            if (displayPoints.size() < 2) continue;

            const presentation::AxisModel xAxis = xAxisFor(s);
            const presentation::AxisModel yAxis = yAxisFor(s);

            QVector<QPointF> pixelPoints;
            pixelPoints.reserve(displayPoints.size());
            for (const auto &p: displayPoints) pixelPoints.append(toPixel(p, rect, xAxis, yAxis));

            SeriesPainter::paint(*painter, pixelPoints, s.color);
        }
    }

    void GraphCanvas::paint(QPainter *painter) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF rect = plotRect();
        drawAxes(painter, rect);
        drawSeries(painter, rect);
    }

    QVariantMap GraphCanvas::valuesAtX(const qreal x) const {
        QVariantMap result;
        result["valid"] = false;
        if (!m_graphVm) return result;

        const auto series = m_graphVm->series(visibleColumnIds());
        if (series.isEmpty()) return result;
        const auto &refSeries = series.front();

        const QRectF rect = plotRect();
        const presentation::AxisModel sharedXAxis = m_graphVm->xAxis();
        const qreal t = rect.width() != 0.0 ? (x - rect.left()) / rect.width() : 0.5;
        const qreal dataX = sharedXAxis.valueAt(t);

        const presentation::AxisModel refXAxis = xAxisFor(refSeries);
        const auto refSample = refSeries.sampleAtX(dataX);
        if (!refSample) return result;

        result["valid"] = true;
        result["x"] = refXAxis.formatTick(refSample->x());
        result["xRaw"] = refSample->x();
        result["pixelX"] = rect.left() + refXAxis.normalizedPosition(refSample->x()) * rect.width();

        QVariantList seriesList;
        for (const auto &s: series) {
            QVariantMap entry;
            entry["name"] = s.name;
            entry["color"] = s.color.name();
            entry["value"] = s.formattedValueAtX(dataX);
            const auto sample = s.sampleAtX(dataX);
            entry["valueRaw"] = sample ? sample->y() : QVariant();
            seriesList.append(entry);
        }
        result["series"] = seriesList;
        return result;
    }

    QVariantMap GraphCanvas::nearestPoint(const qreal x, const qreal y) const {
        QVariantMap best;
        best["valid"] = false;
        if (!m_graphVm) return best;

        const QRectF rect = plotRect();
        const auto series = m_graphVm->series(visibleColumnIds());
        qreal bestDistanceSq = kHoverRadius * kHoverRadius;

        for (const auto &s: series) {
            const presentation::AxisModel xAxis = xAxisFor(s);
            const presentation::AxisModel yAxis = yAxisFor(s);
            const QList<QPointF> displayPoints = s.displayPoints();

            for (const auto &p: displayPoints) {
                const QPointF pixel = toPixel(p, rect, xAxis, yAxis);
                const qreal dx = pixel.x() - x;
                const qreal dy = pixel.y() - y;
                const qreal distSq = dx * dx + dy * dy;
                if (distSq <= bestDistanceSq) {
                    bestDistanceSq = distSq;
                    best["valid"] = true;
                    best["name"] = s.name;
                    best["color"] = s.color.name();
                    best["x"] = xAxis.formatTick(p.x());
                    best["value"] = yAxis.formatTick(p.y());
                }
            }
        }
        return best;
    }
}
