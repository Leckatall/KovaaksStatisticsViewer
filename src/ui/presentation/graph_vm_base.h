//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <cmath>

#include "axis_model.h"
#include "series_model.h"

namespace ksv::presentation {
    // The abstract surface GraphCanvas renders. Concrete view models (the
    // per-run GraphViewModel, the PlaytimeGraphViewModel) implement it so a
    // single painter can draw differently-shaped series - including a date
    // X-axis - without GraphCanvas knowing any one model's column layout.
    //
    // "Columns" are integer series ids. axisBounds() returns a map keyed by
    // the stringified id -> QPointF(min, max); the id returned by xColumn() is
    // the X range and the one from yAxisColumn() drives the Y tick labels.
    class GraphViewModelBase : public QObject {
        Q_OBJECT

    public:
        explicit GraphViewModelBase(QObject *parent = nullptr) : QObject(parent) {}

        // The SeriesModel for each requested column id, in the same order as
        // `columns`; ids with no drawable series (e.g. an X-axis-only column)
        // are simply omitted.
        [[nodiscard]] virtual QList<SeriesModel> series(const QList<int> &columns) const = 0;

        // The shared X axis every series is plotted against.
        [[nodiscard]] virtual AxisModel xAxis() const = 0;

        // DEPRECATED: superseded by series(); kept until
        // DashboardGraphCanvas.qml is migrated off them.

        // Series ids that should be drawn (excludes the X-axis column).
        [[nodiscard]] virtual QVariantList plottableColumns() const = 0;

        // Stringified-id -> QPointF(min, max) for every column, X included.
        [[nodiscard]] virtual QVariantMap axisBounds() const = 0;

        // Nice-number tick positions for `column`, all within that column's
        // bounds. GraphCanvas draws one gridline+label per tick for the X and
        // Y-axis columns. Returns an empty list for columns without an axis.
        [[nodiscard]] virtual QList<qreal> axisTicks(int column) const = 0;

        // {x, value} points for one series id, in draw order.
        [[nodiscard]] virtual QList<QPointF> seriesPoints(int column) const = 0;

        Q_INVOKABLE [[nodiscard]] virtual QString columnName(int column) const = 0;
        Q_INVOKABLE [[nodiscard]] virtual QColor columnColor(int column) const = 0;
        // Stable, identifier-safe key for a column (unlike columnName, never
        // contains spaces) - used to key persisted per-column visibility.
        Q_INVOKABLE [[nodiscard]] virtual QString columnKey(int column) const = 0;

        // Column id whose bounds are the X axis.
        [[nodiscard]] virtual int xColumn() const = 0;
        // Column id whose bounds label the Y axis.
        [[nodiscard]] virtual int yAxisColumn() const = 0;

    signals:
        void boundsChanged();
        // Fires on every data reload, unlike boundsChanged (which only fires
        // when axis bounds actually move) - GraphCanvas uses this to know it
        // must repaint even when the new data's bounds are unchanged.
        void dataUpdated();
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H
