//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H
#define KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <cmath>

#include "axis_model.h"
#include "series_model.h"

namespace ksv::presentation {
    class GraphViewModelBase : public QObject {
        Q_OBJECT

    public:
        explicit GraphViewModelBase(QObject *parent = nullptr) : QObject(parent) {}

        // SeriesModel for each requested column; omits columns with no drawable series.
        // Returned pointers are owned by the VM and remain valid only until its next refresh.
        [[nodiscard]] virtual QList<SeriesModel *> series(const QList<int> &columns) const = 0;

        // Shared X axis all series are plotted against
        [[nodiscard]] virtual AxisModel xAxis() const = 0;

        [[nodiscard]] virtual int yAxisColumn() const = 0;

    signals:
        void boundsChanged();
        void dataUpdated();
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H
