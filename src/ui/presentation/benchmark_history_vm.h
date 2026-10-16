#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>
#include <qqmlintegration.h>

#include <optional>
#include <vector>

#include "axis.h"
#include "axis_policy.h"
#include "graph_vm_base.h"
#include "series_model.h"

namespace ksv::presentation {
    // One drawn metric of the benchmark tracking workspace - either personal-best average rank or
    // averaged playtime. BenchmarkTrackingViewModel owns one instance per metric and hands each its
    // own UTC-day X axis (the same extent unless a range narrows one) while each keeps its own
    // series name, transform, and Y axis.
    // series() is empty whenever the metric is suppressed or has nothing to draw, so an incomplete
    // definition or an empty history never renders a line that resembles data.
    // One tier of the rank axis, bottom (lowest) first; tier k (1-based) sits at y = k.
    struct RankTier {
        QString name;
        QColor color;
    };

    class BenchmarkHistoryViewModel final : public GraphViewModelBase {
        Q_OBJECT
        // Whether line modes have anything to draw: an observation, or a rank carried into the range.
        Q_PROPERTY(bool hasData READ hasData NOTIFY dataUpdated)
        // Whether the unfiltered history has any observation, whatever the range shows.
        Q_PROPERTY(bool historyAvailable READ historyAvailable NOTIFY dataUpdated)
        // Actual observations the range shows (for rank, its improvement points).
        Q_PROPERTY(int observationCount READ observationCount NOTIFY dataUpdated)
        // Whether non-selectable carried geometry holds a rank out to the range's edges.
        Q_PROPERTY(bool hasCarriedState READ hasCarriedState NOTIFY dataUpdated)
        // History exists but the range shows no observation; Points only then draws nothing.
        Q_PROPERTY(bool noObservationsInRange READ noObservationsInRange NOTIFY dataUpdated)
        Q_PROPERTY(QString emptyStateText READ emptyStateText NOTIFY dataUpdated)
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
        [[nodiscard]] bool historyAvailable() const { return m_historyAvailable; }
        [[nodiscard]] int observationCount() const { return m_observationCount; }
        [[nodiscard]] bool hasCarriedState() const { return m_hasCarriedState; }
        [[nodiscard]] bool noObservationsInRange() const { return m_historyAvailable && m_observationCount == 0; }
        [[nodiscard]] QString emptyStateText() const {
            return m_historyAvailable ? tr("No history in the selected range.") : tr("No history recorded yet.");
        }
        [[nodiscard]] QString metricName() const;

        // Only meaningful for RollingPlaytime; takes effect on the next update().
        void setAveragingWindowDays(int days);

        // Takes the "playtime" policy, in minutes, for RollingPlaytime and is ignored for AverageRank:
        // the rank axis is always tier-based. Kept across updates.
        void setAxisPolicies(const AxisPolicyMap &policies);

        // This metric's graph style: connection, markers, series colour and width, and the playtime
        // axis policy. Kept across updates; the series name never depends on it.
        void setPresentation(const application::GraphPresentation &presentation);
        [[nodiscard]] int averagingWindowDays() const { return m_windowDays; }

        // X is UTC-midnight epoch ms, Y the metric's raw value (rank position, or playtime seconds).
        struct Content {
            QList<QPointF> observations;
            std::optional<QPointF> leadingCarry;
            std::optional<QPointF> trailingCarry;
            // Highest value of the unfiltered history: tier references keep their full-history
            // meaning rather than fitting only what a narrowed range shows.
            qreal fullHistoryPeak = 0.0;
            bool historyAvailable = false;
        };

        // Called by BenchmarkTrackingViewModel inside one atomic rebuild, after it has updated this
        // metric's X axis. `present` is false when the metric is suppressed; `rankTiers` is only read
        // for AverageRank: it sizes the rank axis from 0 up to one tier above the full-history peak
        // (capped at the top tier), with a tick at every shown tier, and gives each shown tier a
        // reference line in its colour. The rank line itself has no colour of its own (neutral), so
        // it is never mistaken for a tier line.
        void update(const Content &content, bool present, const std::vector<RankTier> &rankTiers);

    signals:
        void metricNameChanged();

    private:
        [[nodiscard]] application::GraphKey graphKey() const {
            return m_metric == AverageRank ? application::GraphKey::BenchmarkRank
                                           : application::GraphKey::BenchmarkPlaytime;
        }
        [[nodiscard]] static ValueAxis rankYAxis(const std::vector<RankTier> &tiers);
        void applyPlaytimeYAxis();

        Metric m_metric;
        int m_windowDays = 3;
        application::GraphPresentation m_presentation;
        application::AxisPolicy m_playtimeAxisPolicy;
        SeriesModel *m_series;
        const Axis *m_xAxis;
        bool m_hasData = false;
        bool m_historyAvailable = false;
        int m_observationCount = 0;
        bool m_hasCarriedState = false;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H
