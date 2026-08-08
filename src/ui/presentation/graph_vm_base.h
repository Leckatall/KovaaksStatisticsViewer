//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <cmath>

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

        [[nodiscard]] virtual int pointCount() const = 0;

        // Series ids that should be drawn (excludes the X-axis column).
        [[nodiscard]] virtual QVariantList plottableColumns() const = 0;

        // Stringified-id -> QPointF(min, max) for every column, X included.
        [[nodiscard]] virtual QVariantMap axisBounds() const = 0;

        // {x, value} points for one series id, in draw order.
        [[nodiscard]] virtual QList<QPointF> seriesPoints(int column) const = 0;

        Q_INVOKABLE [[nodiscard]] virtual QString columnName(int column) const = 0;
        Q_INVOKABLE [[nodiscard]] virtual QColor columnColor(int column) const = 0;

        // Column id whose bounds are the X axis.
        [[nodiscard]] virtual int xColumn() const = 0;
        // Column id whose bounds label the Y axis.
        [[nodiscard]] virtual int yAxisColumn() const = 0;

        // Axis tick label formatting. Default is a compact number; override
        // formatXTick to render dates, etc.
        [[nodiscard]] virtual QString formatXTick(qreal value) const { return defaultTick(value); }
        [[nodiscard]] virtual QString formatYTick(qreal value) const { return defaultTick(value); }

    signals:
        void boundsChanged();
        void pointCountChanged();

    protected:
        // Whole numbers print with no decimals, everything else with one.
        static QString defaultTick(const qreal value) {
            return QString::number(value, 'f', std::abs(value - std::round(value)) < 1e-6 ? 0 : 1);
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H
