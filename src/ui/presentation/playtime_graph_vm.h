//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_VM_H
#define KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_VM_H

#include <QList>
#include <QPointF>
#include <memory>
#include <qqmlintegration.h>

#include "graph_vm_base.h"
#include "app/usecases/i_playtime_graph_use_case.h"

namespace ksv::presentation {
    // Days-vs-playtime graph: X is a calendar date, Y is the 3-day rolling
    // average of that day's total playtime in minutes, across all scenarios.
    // Pulls its series from IPlaytimeGraphUseCase and reshapes it for the
    // shared GraphCanvas painter (one series, dated X axis).
    class PlaytimeGraphViewModel : public GraphViewModelBase {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Created in C++")
        Q_PROPERTY(int pointCount READ pointCount NOTIFY pointCountChanged)

    public:
        // Column ids for the two axes; Playtime is the only drawn series.
        enum Column { Date = 0, Playtime = 1 };

        explicit PlaytimeGraphViewModel(std::shared_ptr<application::IPlaytimeGraphUseCase> useCase,
                                        QObject *parent = nullptr);

        [[nodiscard]] int pointCount() const override { return int(m_points.size()); }
        [[nodiscard]] QVariantList plottableColumns() const override { return {int(Playtime)}; }
        [[nodiscard]] QVariantMap axisBounds() const override;
        [[nodiscard]] QList<QPointF> seriesPoints(int column) const override;
        Q_INVOKABLE [[nodiscard]] QString columnName(int column) const override;
        Q_INVOKABLE [[nodiscard]] QColor columnColor(int column) const override;

        [[nodiscard]] int xColumn() const override { return Date; }
        [[nodiscard]] int yAxisColumn() const override { return Playtime; }

        [[nodiscard]] QString formatXTick(qreal value) const override;

    public slots:
        // Re-pulls the rolling average from the use case. Call after the
        // profile reloads.
        void refresh();

    private:
        // Trailing window for the rolling average, in days.
        static constexpr int kWindowDays = 3;

        std::shared_ptr<application::IPlaytimeGraphUseCase> m_useCase;
        // {days-since-epoch, minutes} points, ordered by day.
        QList<QPointF> m_points;
        std::pair<qreal, qreal> m_xBounds{0.0, 1.0};
        std::pair<qreal, qreal> m_yBounds{0.0, 1.0};
    };
}

#endif //KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_VM_H
