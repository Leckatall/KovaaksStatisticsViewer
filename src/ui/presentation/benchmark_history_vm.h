#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>
#include <qqmlintegration.h>

#include <vector>

#include "axis.h"
#include "graph_vm_base.h"
#include "series_model.h"

namespace ksv::presentation {
    // One drawn metric of the benchmark tracking workspace - either personal-best average rank or
    // averaged playtime. BenchmarkTrackingViewModel owns one instance per metric and hands each a
    // UTC-day X axis (the same extent unless the playtime range narrows it) while each keeps its own
    // series name, transform, and Y axis.
    // series() is empty whenever the metric is suppressed or has no points, so an incomplete
    // definition or an empty history never renders a line that resembles data.
    // One tier of the rank axis, bottom (lowest) first; tier k (1-based) sits at y = k.
    struct RankTier {
        QString name;
        QColor color;
    };

    class BenchmarkHistoryViewModel final : public GraphViewModelBase {
        Q_OBJECT
        Q_PROPERTY(bool hasData READ hasData NOTIFY dataUpdated)
        Q_PROPERTY(QString emptyStateText READ emptyStateText CONSTANT)
        // Names the playtime metric's effective averaging window; the rank metric's name never changes.
        Q_PROPERTY(QString metricName READ metricName NOTIFY metricNameChanged)

    public:
        enum Metric { AverageRank, RollingPlaytime };
        Q_ENUM(Metric)

        // Date is axis-only; Value is the single drawn series (mirrors PlaytimeGraphViewModel).
        enum Column { Date = 0, Value = 1 };
        Q_ENUM(Column)

        // `xAxis` is owned by the parent BenchmarkTrackingViewModel and must outlive this object.
        BenchmarkHistoryViewModel(Metric metric, const Axis &xAxis, QObject *parent = nullptr);

        [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override;
        [[nodiscard]] const Axis &xAxis() const override { return *m_xAxis; }
        [[nodiscard]] int yAxisColumn() const override { return Value; }

        [[nodiscard]] bool hasData() const { return m_hasData; }
        [[nodiscard]] QString emptyStateText() const { return tr("No history recorded yet."); }
        [[nodiscard]] QString metricName() const;

        // Only meaningful for RollingPlaytime; takes effect on the next update().
        void setAveragingWindowDays(int days);
        [[nodiscard]] int averagingWindowDays() const { return m_windowDays; }

        // Called by BenchmarkTrackingViewModel inside one atomic rebuild, after it has updated the
        // shared X axis. `rawPoints` carry UTC-midnight epoch-ms X and the metric's raw Y (rank
        // position, or playtime seconds). `present` is false when the metric is suppressed;
        // `rankTiers` is only read for AverageRank: it sizes the rank axis from 0 up to one tier
        // above the peak rank (capped at the top tier), with a tick at every shown tier, and gives
        // each shown tier a reference line in its colour. The rank line itself
        // has no colour of its own (neutral), so it is never mistaken for a tier line.
        void update(const QList<QPointF> &rawPoints, bool present, const std::vector<RankTier> &rankTiers);

    signals:
        void metricNameChanged();

    private:
        [[nodiscard]] static ValueAxis rankYAxis(const std::vector<RankTier> &tiers);

        Metric m_metric;
        int m_windowDays = 3;
        SeriesModel *m_series;
        const Axis *m_xAxis;
        bool m_hasData = false;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H
