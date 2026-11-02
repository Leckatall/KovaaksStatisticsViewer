//
// Created by Lecka on 07/08/2026.
//

#include "graph_canvas.h"

#include <QColor>
#include <algorithm>
#include <cmath>
#include <limits>

#include "graph_scene_painter.h"
#include "series_painter.h"

namespace ksv::ui {
    namespace {
        constexpr qreal kHoverRadius = 10;

        GraphSceneAxis resolveAxis(const presentation::Axis &axis) {
            GraphSceneAxis resolved;
            resolved.min = axis.min();
            resolved.max = axis.max();
            resolved.ticks = axis.ticks();
            resolved.labels.reserve(resolved.ticks.size());
            for (const qreal tick: resolved.ticks) resolved.labels.append(axis.formatTick(tick));
            resolved.minLabel = axis.formatTick(resolved.min);
            resolved.maxLabel = axis.formatTick(resolved.max);
            return resolved;
        }

        GraphSceneAxis resolveBounds(const presentation::Axis &axis) {
            GraphSceneAxis resolved;
            resolved.min = axis.min();
            resolved.max = axis.max();
            resolved.minLabel = axis.formatTick(resolved.min);
            resolved.maxLabel = axis.formatTick(resolved.max);
            return resolved;
        }

        std::optional<QPointF> displayCarry(const presentation::SeriesModel &series,
                                            const std::optional<QPointF> &raw) {
            if (!raw) return std::nullopt;
            return QPointF(raw->x(), series.transform.display(raw->y()));
        }
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
        return graphMarkersFit(pointCount, pixelSpan);
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
        const QRectF bounds(0, 0, width(), height());
        if (!m_graphVm) return graphPlotRect(bounds, std::nullopt, std::nullopt);
        std::optional<GraphSceneAxis> yAxis;
        if (const auto labelled = labelledYAxis()) yAxis = resolveAxis(*labelled);
        // Runs on every hover move. The bottom margin comes from the font alone, so no x label is formatted.
        return graphPlotRect(bounds, yAxis, GraphSceneAxis{});
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

    void GraphCanvas::paint(QPainter *painter) {
        paintGraphScene(*painter, scene(), QRectF(0, 0, width(), height()));
    }

    GraphScene GraphCanvas::scene() const {
        GraphScene scene;
        scene.connection = painterConnection();
        scene.pointMarkers = m_pointMarkers;
        scene.background = m_backgroundColor;
        if (!m_graphVm) return scene;

        scene.xAxis = resolveAxis(m_graphVm->xAxis());
        if (const auto yAxis = labelledYAxis()) scene.labelledYAxis = resolveAxis(*yAxis);
        scene.labelledColumn = labelledYAxisColumn();
        for (const auto *s: labelledAxisGroup().members) scene.labelledColumns.append(s->column());
        scene.labelledYAxisTitle = labelledYAxisTitle();
        if (std::isfinite(m_markedX)) scene.selectedBand = GraphSceneBand{m_markedX, m_markerColor};

        const auto series = m_graphVm->series(visibleColumnIds());
        scene.series.reserve(series.size());
        for (const auto *s: series) {
            GraphSceneSeries entry;
            entry.column = s->column();
            entry.id = s->id();
            entry.name = s->name();
            entry.color = colorFor(*s);
            entry.lineWidth = s->lineWidth;
            entry.observations = s->displayPoints();
            entry.leadingCarry = displayCarry(*s, s->leadingCarry);
            entry.trailingCarry = displayCarry(*s, s->trailingCarry);
            entry.xAxis = s->xAxis ? resolveBounds(*s->xAxis) : scene.xAxis;
            entry.yAxis = resolveBounds(yAxisFor(*s));
            entry.yAxisId = s->yAxisId;
            entry.yAxisName = s->yAxisName;
            entry.references.reserve(s->referenceLines.size());
            for (const auto &line: s->referenceLines) entry.references.append({line.value, line.color});
            scene.series.append(std::move(entry));
        }
        return scene;
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
