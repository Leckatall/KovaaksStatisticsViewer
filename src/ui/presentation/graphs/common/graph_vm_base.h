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

#include "app/contracts/graph_presentation.h"
#include "presentation/graphs/common/axes/axis.h"
#include "presentation/graphs/common/series/series_model.h"

namespace ksv::presentation {
    class GraphViewModelBase : public QObject {
        Q_OBJECT
        // GraphCanvas::ConnectionMode: 0 smooth, 1 straight, 2 points only.
        Q_PROPERTY(int connectionMode READ connectionMode NOTIFY styleChanged)
        // The line modes' marker preference; points only marks every observation regardless.
        Q_PROPERTY(bool pointMarkers READ pointMarkers NOTIFY styleChanged)

    public:
        explicit GraphViewModelBase(QObject *parent = nullptr) : QObject(parent) {}

        // SeriesModel for each requested column, in the graph's line-list order (first = top of the
        // list, drawn on top) whatever order the columns are requested in; omits columns with no
        // drawable series.
        // Returned pointers are owned by the VM and remain valid only until its next refresh.
        [[nodiscard]] virtual QList<SeriesModel *> series(const QList<int> &columns) const = 0;

        // Shared X axis all series are plotted against
        [[nodiscard]] virtual const Axis &xAxis() const = 0;

        [[nodiscard]] virtual int yAxisColumn() const = 0;

        [[nodiscard]] int connectionMode() const { return static_cast<int>(m_appearance.connection); }
        [[nodiscard]] bool pointMarkers() const { return m_appearance.lineModeMarkers; }

    signals:
        void boundsChanged();
        void dataUpdated();
        // Connection mode, markers, series colours or widths changed; the data did not.
        void styleChanged();

    protected:
        void setAppearance(const application::LineAppearance &appearance) { m_appearance = appearance; }

    private:
        application::LineAppearance m_appearance;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_VM_BASE_H
