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
    class GraphViewModelBase : public QObject {
        Q_OBJECT

    public:
        explicit GraphViewModelBase(QObject *parent = nullptr) : QObject(parent) {}

        // SeriesModel for each requested column; omits columns with no drawable series
        [[nodiscard]] virtual QList<SeriesModel> series(const QList<int> &columns) const = 0;

        // Shared X axis all series are plotted against
        [[nodiscard]] virtual AxisModel xAxis() const = 0;

        // TODO(2026-08-14): Remove with the remaining legacy graph-interface methods.
        [[nodiscard]] virtual QVariantList plottableColumns() const { return {}; }

        [[nodiscard]] virtual QVariantMap axisBounds() const = 0;

        [[nodiscard]] virtual QList<qreal> axisTicks(int column) const = 0;

        [[nodiscard]] virtual QList<QPointF> seriesPoints(int column) const = 0;

        Q_INVOKABLE [[nodiscard]] virtual QString columnName(int column) const = 0;
        Q_INVOKABLE [[nodiscard]] virtual QColor columnColor(int column) const = 0;
        Q_INVOKABLE [[nodiscard]] virtual QString columnKey(int column) const = 0;

        [[nodiscard]] virtual int xColumn() const = 0;
        [[nodiscard]] virtual int yAxisColumn() const = 0;

    signals:
        void boundsChanged();
        void dataUpdated();
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H
