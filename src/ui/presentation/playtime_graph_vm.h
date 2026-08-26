//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_VM_H
#define KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_VM_H

#include <QList>
#include <QPointF>
#include <memory>
#include <qqmlintegration.h>

#include "axis_model.h"
#include "graph_vm_base.h"
#include "app/contracts/i_playtime_graph_use_case.h"

namespace ksv::presentation {
    // Calendar-date vs rolling-average playtime. Adapts IPlaytimeGraphUseCase output for GraphCanvas.
    class PlaytimeGraphViewModel : public GraphViewModelBase {
        Q_OBJECT

    public:
        // Column ids for the two axes; Playtime is the only drawn series.
        enum Column { Date = 0, Playtime = 1 };
        Q_ENUM(Column)

        explicit PlaytimeGraphViewModel(std::shared_ptr<application::IPlaytimeGraphUseCase> useCase,
                                        QObject *parent = nullptr);

        [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override {
            QList<SeriesModel *> result;
            for (const int c: columns) if (c == Playtime) result.append(m_series);
            return result;
        }
        [[nodiscard]] AxisModel xAxis() const override { return m_xAxis; }

        [[nodiscard]] int yAxisColumn() const override { return Playtime; }

    public slots:
        // Re-pulls rolling average when profile reloads
        void refresh();

    private:
        // TODO: make this configurable by the user
        // Trailing window for the rolling average, in days.
        static constexpr int kWindowDays = 3;

        std::shared_ptr<application::IPlaytimeGraphUseCase> m_useCase;
        AxisModel m_xAxis;
        SeriesModel *m_series; // raw seconds + a seconds->minutes transform
    };
}

#endif //KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_VM_H
