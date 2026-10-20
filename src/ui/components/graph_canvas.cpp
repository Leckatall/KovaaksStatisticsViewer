//
// Created by Lecka on 07/08/2026.
//

#include "graph_canvas.h"

#include <QColor>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <limits>

#include "axis_painter.h"
#include "series_painter.h"

namespace ksv::ui {
    namespace {
        constexpr qreal kMinLeftMargin = 20;
        constexpr qreal kMinBottomMargin = 16;
        constexpr qreal kTopMargin = 10;
        constexpr qreal kRightMargin = 20;
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
            connect(m_graphVm, &presentation::GraphViewModelBase::dataUpdated,
                    this, [this] {
                        emit labelledYAxisColumnChanged();
                        emit plotAreaChanged();
                        update();
                    });
            connect(m_graphVm, &presentation::GraphViewModelBase::boundsChanged,
                    this, [this] {
                        emit labelledYAxisColumnChanged();
                        emit plotAreaChanged();
                        update();
                    });
            connect(m_graphVm, &presentation::GraphViewModelBase::styleChanged,
                    this, [this] {
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

    void GraphCanvas::setPointMarkers(const bool pointMarkers) {
        if (m_pointMarkers == pointMarkers) return;
        m_pointMarkers = pointMarkers;
        emit pointMarkersChanged();
        emit plotAreaChanged();
        update();
    }

    void GraphCanvas::setConnectionMode(const ConnectionMode mode) {
        if (m_connectionMode == mode) return;
        m_connectionMode = mode;
        emit connectionModeChanged();
        emit plotAreaChanged();
        update();
    }

    void GraphCanvas::setBackgroundColor(const QColor &backgroundColor) {
        if (m_backgroundColor == backgroundColor) return;
        m_backgroundColor = backgroundColor;
        emit backgroundColorChanged();
        update();
    }

    void GraphCanvas::setForegroundColor(const QColor &foregroundColor) {
        if (m_foregroundColor == foregroundColor) return;
        m_foregroundColor = foregroundColor;
        emit foregroundColorChanged();
        update();
    }

    SeriesPainter::Connection GraphCanvas::painterConnection() const {
        switch (m_connectionMode) {
            case ConnectionMode::Straight: return SeriesPainter::Connection::Straight;
            case ConnectionMode::PointsOnly: return SeriesPainter::Connection::None;
            case ConnectionMode::Smooth: break;
        }
        return SeriesPainter::Connection::Smooth;
    }

    QColor GraphCanvas::colorFor(const presentation::SeriesModel &series) const {
        return series.color().isValid() ? series.color() : m_foregroundColor;
    }

    void GraphCanvas::setMarkedX(const qreal markedX) {
        const bool bothNone = std::isnan(m_markedX) && std::isnan(markedX);
        if (bothNone || m_markedX == markedX) return;
        m_markedX = markedX;
        emit markedXChanged();
        update();
    }

    void GraphCanvas::setMarkerColor(const QColor &markerColor) {
        if (m_markerColor == markerColor) return;
        m_markerColor = markerColor;
        emit markerColorChanged();
        update();
    }

    bool GraphCanvas::markersFit(const int pointCount, const qreal pixelSpan) {
        if (pointCount < 2) return pointCount == 1;
        return pixelSpan / (pointCount - 1) >= kMinMarkerSpacing;
    }

    bool GraphCanvas::pointMarkersShown() const {
        const bool pointsOnly = m_connectionMode == ConnectionMode::PointsOnly;
        if (!(m_pointMarkers || pointsOnly) || !m_graphVm) return false;
        const QRectF rect = plotRect();
        const auto series = m_graphVm->series(visibleColumnIds());
        bool any = false;
        for (const auto *s: series) {
            if (s->points.isEmpty()) continue;
            if (pointsOnly || s->points.size() == 1) {
                any = true;
                continue;
            }
            const presentation::Axis &xAxis = xAxisFor(*s);
            const qreal span = (xAxis.normalizedPosition(s->points.back().x()) -
                                xAxis.normalizedPosition(s->points.front().x())) * rect.width();
            if (!markersFit(int(s->points.size()), std::abs(span))) return false;
            any = true;
        }
        return any;
    }

    GraphCanvas::LabelledAxisGroup GraphCanvas::labelledAxisGroup() const {
        LabelledAxisGroup group;
        if (!m_graphVm) return group;
        const int labelledColumn = labelledYAxisColumn();
        const auto series = m_graphVm->series(visibleColumnIds());
        const auto labelled = std::ranges::find_if(series, [labelledColumn](const auto *s) {
            return s->column() == labelledColumn;
        });
        if (labelled == series.end()) return group;
        group.labelled = *labelled;
        const auto &sharedAxis = group.labelled->yAxisId;
        for (const auto *s: series) {
            if (s == group.labelled || (sharedAxis && s->yAxisId == sharedAxis)) group.members.append(s);
        }
        return group;
    }

    QVariantList GraphCanvas::labelledYAxisColumns() const {
        QVariantList columns;
        for (const auto *s: labelledAxisGroup().members) columns.append(s->column());
        return columns;
    }

    QString GraphCanvas::labelledYAxisTitle() const {
        const auto group = labelledAxisGroup();
        if (!group.labelled) return {};
        if (!group.labelled->yAxisId) return group.labelled->name();
        if (!group.labelled->yAxisName.isEmpty()) return group.labelled->yAxisName;
        QStringList names;
        for (const auto *s: group.members) names.append(s->name());
        return names.join(QStringLiteral(", "));
    }

    QList<const presentation::SeriesModel *> GraphCanvas::seriesInPaintOrder() const {
        QList<const presentation::SeriesModel *> ordered;
        if (!m_graphVm) return ordered;
        // series() is in the user's line-list order; painting it back to front leaves the series
        // highest in the list on top.
        const auto series = m_graphVm->series(visibleColumnIds());
        for (auto it = series.crbegin(); it != series.crend(); ++it) ordered.append(*it);
        return ordered;
    }

    QList<int> GraphCanvas::paintOrder() const {
        QList<int> columns;
        for (const auto *s: seriesInPaintOrder()) columns.append(s->column());
        return columns;
    }

    QList<int> GraphCanvas::visibleColumnIds() const {
        QList<int> ids;
        ids.reserve(m_visibleColumns.size());
        for (const auto &v: m_visibleColumns) ids.append(v.toInt());
        return ids;
    }

    std::optional<presentation::ValueAxis> GraphCanvas::labelledYAxis() const {
        if (!m_graphVm) return std::nullopt;
        const int labelledColumn = labelledYAxisColumn();
        const QList<int> visible = visibleColumnIds();
        const auto series = m_graphVm->series(visible);
        for (const auto *entry: series) {
            if (entry->column() == labelledColumn) return yAxisFor(*entry);
        }
        const auto fallback = m_graphVm->series({labelledColumn});
        if (fallback.isEmpty()) return std::nullopt;
        return yAxisFor(*fallback.front());
    }

    QRectF GraphCanvas::plotRect() const {
        qreal leftMargin = kMinLeftMargin;
        qreal bottomMargin = kMinBottomMargin;

        if (m_graphVm) {
            if (const auto yAxis = labelledYAxis()) {
                leftMargin = std::max(kMinLeftMargin,
                                      AxisPainter::measureLabelExtent(AxisPainter::Orientation::Vertical,
                                                                      yAxis->ticks(),
                                                                      [&yAxis](const qreal v) {
                                                                          return yAxis->formatTick(v);
                                                                      }) + kLabelExtentPadding);
            }
            const presentation::Axis &xAxis = m_graphVm->xAxis();
            bottomMargin = std::max(kMinBottomMargin,
                                    AxisPainter::measureLabelExtent(AxisPainter::Orientation::Horizontal, xAxis.ticks(),
                                                                    [&xAxis](const qreal v) {
                                                                        return xAxis.formatTick(v);
                                                                    }) + kLabelExtentPadding);
        }

        return {
            leftMargin, kTopMargin,
            qMax(0.0, width() - leftMargin - kRightMargin),
            qMax(0.0, height() - kTopMargin - bottomMargin)
        };
    }

    const presentation::Axis &GraphCanvas::xAxisFor(const presentation::SeriesModel &series) const {
        return series.xAxis ? *series.xAxis : m_graphVm->xAxis();
    }

    presentation::ValueAxis GraphCanvas::yAxisFor(const presentation::SeriesModel &series) const {
        return series.yAxis.value_or(series.deriveYAxis());
    }

    QPointF GraphCanvas::toPixel(const QPointF &displayPoint, const QRectF &rect,
                                 const presentation::Axis &xAxis, const presentation::Axis &yAxis) {
        const qreal xt = xAxis.normalizedPosition(displayPoint.x());
        const qreal yt = yAxis.normalizedPosition(displayPoint.y());
        return {rect.left() + xt * rect.width(), rect.bottom() - yt * rect.height()};
    }

    void GraphCanvas::drawAxes(QPainter *painter, const QRectF &rect) const {
        if (!m_graphVm) return;
        const presentation::Axis &xAxis = m_graphVm->xAxis();

        const QRectF bounds(0, 0, width(), height());

        // Only one series' Y axis gets labels; all project against their own axis.
        if (const auto yAxis = labelledYAxis()) {
            AxisPainter::paint(*painter, rect, bounds, AxisPainter::Orientation::Vertical,
                               yAxis->min(), yAxis->max(), yAxis->ticks(),
                               [&yAxis](const qreal v) { return yAxis->formatTick(v); });
        }
        AxisPainter::paint(*painter, rect, bounds, AxisPainter::Orientation::Horizontal,
                           xAxis.min(), xAxis.max(), xAxis.ticks(),
                           [&xAxis](const qreal v) { return xAxis.formatTick(v); });
    }

    void GraphCanvas::drawSeries(QPainter *painter, const QRectF &rect) const {
        if (!m_graphVm) return;

        // Each line's markers are painted with it, so they stack the same way as the lines.
        for (const auto *s: seriesInPaintOrder()) {
            const QList<QPointF> displayPoints = s->displayPoints();
            if (displayPoints.isEmpty() && !s->leadingCarry && !s->trailingCarry) continue;

            const presentation::Axis &xAxis = xAxisFor(*s);
            const presentation::ValueAxis yAxis = yAxisFor(*s);
            const auto carryPixel = [&](const std::optional<QPointF> &raw) -> std::optional<QPointF> {
                if (!raw) return std::nullopt;
                return toPixel(QPointF(raw->x(), s->transform.display(raw->y())), rect, xAxis, yAxis);
            };

            QVector<QPointF> pixelPoints;
            pixelPoints.reserve(displayPoints.size());
            for (const auto &p: displayPoints) pixelPoints.append(toPixel(p, rect, xAxis, yAxis));

            SeriesPainter::Style style;
            style.lineWidth = s->lineWidth;
            style.connection = painterConnection();
            // A line centred on the plot edge keeps its full stroke; anything further out is cut.
            const qreal edgeAllowance = s->lineWidth / 2;
            style.clip = rect.adjusted(-edgeAllowance, -edgeAllowance, edgeAllowance, edgeAllowance);
            style.markerOutlineColor = m_backgroundColor;
            style.showMarkers = m_pointMarkers && !pixelPoints.isEmpty() &&
                                markersFit(int(pixelPoints.size()), pixelPoints.back().x() - pixelPoints.front().x());
            SeriesPainter::paint(*painter, pixelPoints, {carryPixel(s->leadingCarry), carryPixel(s->trailingCarry)},
                                 colorFor(*s), style);
        }
    }

    void GraphCanvas::drawReferenceLines(QPainter *painter, const QRectF &rect) const {
        if (!m_graphVm) return;
        painter->save();
        for (const auto *s: seriesInPaintOrder()) {
            if (s->referenceLines.isEmpty()) continue;
            const presentation::ValueAxis yAxis = yAxisFor(*s);
            for (const auto &line: s->referenceLines) {
                const qreal t = yAxis.normalizedPosition(line.value);
                if (t < 0.0 || t > 1.0) continue;
                const qreal y = rect.bottom() - t * rect.height();
                QPen pen(line.color, kReferenceLineWidth);
                pen.setCapStyle(Qt::FlatCap);
                painter->setPen(pen);
                painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
            }
        }
        painter->restore();
    }

    void GraphCanvas::drawMarkedX(QPainter *painter, const QRectF &rect) const {
        if (!m_graphVm || std::isnan(m_markedX)) return;
        const presentation::Axis &sharedXAxis = m_graphVm->xAxis();
        const qreal t = sharedXAxis.normalizedPosition(m_markedX);
        if (t < 0.0 || t > 1.0) return;
        const qreal pixelX = rect.left() + t * rect.width();

        painter->save();
        QPen rulePen(m_markerColor, 2.0);
        rulePen.setCapStyle(Qt::FlatCap);
        painter->setPen(rulePen);
        painter->drawLine(QPointF(pixelX, rect.top()), QPointF(pixelX, rect.bottom()));

        // A ring in the marker colour around each visible line's point at the marked x, over the
        // line's own colour, so the point reads as "this run" on every line.
        constexpr qreal kRingRadius = 6.0;
        for (const auto *s: seriesInPaintOrder()) {
            const auto sample = s->sampleAtX(m_markedX);
            if (!sample || sample->x() != m_markedX) continue;
            const QPointF pixel = toPixel(*sample, rect, xAxisFor(*s), yAxisFor(*s));
            painter->setPen(QPen(m_markerColor, 2.5));
            painter->setBrush(colorFor(*s));
            painter->drawEllipse(pixel, kRingRadius, kRingRadius);
        }
        painter->restore();
    }

    void GraphCanvas::paint(QPainter *painter) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF rect = plotRect();
        drawAxes(painter, rect);
        drawReferenceLines(painter, rect);
        drawSeries(painter, rect);
        drawMarkedX(painter, rect);
    }

    QVariantMap GraphCanvas::valuesAtX(const qreal x) const {
        QVariantMap result;
        result["valid"] = false;
        if (!m_graphVm) return result;

        const auto series = m_graphVm->series(visibleColumnIds());
        if (series.isEmpty()) return result;
        const auto &refSeries = *series.front();

        const QRectF rect = plotRect();
        const presentation::Axis &sharedXAxis = m_graphVm->xAxis();
        const qreal t = rect.width() != 0.0 ? (x - rect.left()) / rect.width() : 0.5;
        const qreal dataX = sharedXAxis.valueAt(t);

        const presentation::Axis &refXAxis = xAxisFor(refSeries);
        const auto refSample = refSeries.sampleAtX(dataX);
        if (!refSample) return result;

        result["valid"] = true;
        result["x"] = refXAxis.formatTick(refSample->x());
        result["xRaw"] = refSample->x();
        result["pixelX"] = rect.left() + refXAxis.normalizedPosition(refSample->x()) * rect.width();

        QVariantList seriesList;
        for (const auto *s: series) {
            QVariantMap entry;
            entry["name"] = s->name();
            entry["color"] = colorFor(*s);
            entry["value"] = s->formattedValueAtX(dataX);
            const auto sample = s->sampleAtX(dataX);
            entry["valueRaw"] = sample ? sample->y() : QVariant();
            entry["pixelY"] = sample ? toPixel(*sample, rect, xAxisFor(*s), yAxisFor(*s)).y() : QVariant();
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

        for (const auto *s: series) {
            const presentation::Axis &xAxis = xAxisFor(*s);
            const presentation::ValueAxis yAxis = yAxisFor(*s);
            const QList<QPointF> displayPoints = s->displayPoints();

            for (const auto &p: displayPoints) {
                const QPointF pixel = toPixel(p, rect, xAxis, yAxis);
                const qreal dx = pixel.x() - x;
                const qreal dy = pixel.y() - y;
                const qreal distSq = dx * dx + dy * dy;
                if (distSq <= bestDistanceSq) {
                    bestDistanceSq = distSq;
                    best["valid"] = true;
                    best["name"] = s->name();
                    best["color"] = colorFor(*s).name();
                    best["x"] = xAxis.formatTick(p.x());
                    best["value"] = yAxis.formatTick(p.y());
                }
            }
        }
        return best;
    }
}
