#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H

#include <QList>
#include <QPointF>
#include <QString>
#include <qqmlintegration.h>

#include <vector>

#include "axis_model.h"
#include "graph_vm_base.h"
#include "series_model.h"

namespace ksv::presentation {
    // One drawn metric of the benchmark tracking workspace - either personal-best average rank or
    // three-day average playtime. BenchmarkTrackingViewModel owns one instance per metric and drives
    // both with the same UTC-day X axis while each keeps its own series name, transform, and Y axis.
    // series() is empty whenever the metric is suppressed or has no points, so an incomplete
    // definition or an empty history never renders a line that resembles data.
    class BenchmarkHistoryViewModel final : public GraphViewModelBase {
        Q_OBJECT
        Q_PROPERTY(bool hasData READ hasData NOTIFY dataUpdated)
        Q_PROPERTY(QString emptyStateText READ emptyStateText CONSTANT)
        Q_PROPERTY(QString metricName READ metricName CONSTANT)

    public:
        enum Metric { AverageRank, RollingPlaytime };
        Q_ENUM(Metric)

        // Date is axis-only; Value is the single drawn series (mirrors PlaytimeGraphViewModel).
        enum Column { Date = 0, Value = 1 };
        Q_ENUM(Column)

        explicit BenchmarkHistoryViewModel(Metric metric, QObject *parent = nullptr);

        [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override;
        [[nodiscard]] AxisModel xAxis() const override { return m_xAxis; }
        [[nodiscard]] int yAxisColumn() const override { return Value; }

        [[nodiscard]] bool hasData() const { return m_hasData; }
        [[nodiscard]] QString emptyStateText() const { return tr("No history recorded yet."); }
        [[nodiscard]] QString metricName() const {
            return m_metric == AverageRank ? tr("Personal-best average rank")
                                           : tr("Three-day average playtime");
        }

        // Called by BenchmarkTrackingViewModel inside one atomic rebuild. `rawPoints` carry
        // UTC-midnight epoch-ms X and the metric's raw Y (rank position, or playtime seconds).
        // `present` is false when the metric is suppressed; `rankTierNames` is only read for
        // AverageRank and sizes the 0..tierCount rank axis.
        void update(const QList<QPointF> &rawPoints, const AxisModel &sharedXAxis, bool present,
                    const std::vector<QString> &rankTierNames);

    private:
        [[nodiscard]] static AxisModel rankYAxis(const std::vector<QString> &tierNames);

        Metric m_metric;
        SeriesModel *m_series;
        AxisModel m_xAxis;
        bool m_hasData = false;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H
