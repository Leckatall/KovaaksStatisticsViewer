#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>
#include <qqmlintegration.h>

#include <optional>
#include <vector>

#include "presentation/graphs/common/axes/axis.h"
#include "presentation/graphs/common/axes/axis_policy.h"
#include "presentation/graphs/common/axes/date_time_axis.h"
#include "presentation/graphs/common/axes/run_date_axis.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"

namespace ksv::presentation {
    // One drawn metric of the benchmark tracking workspace - personal-best average rank, recent
    // average rank, or averaged playtime. BenchmarkTrackingViewModel owns one instance per metric and
    // sets the range of the X axes each one owns: a UTC-day calendar axis, and a run axis that only a
    // rank metric is ever shown on. xAxis() is whichever the last update() was laid out for.
    // series() is empty whenever the metric is suppressed or has nothing to draw, so an incomplete
    // definition or an empty history never renders a line that resembles data.
    // One tier of the rank axis, bottom (lowest) first; tier k (1-based) sits at y = k.
    struct RankTier {
        QString name;
        QColor color;
    };

    class BenchmarkHistoryViewModel final : public GraphViewModelBase {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Created in C++")
        // Whether line modes have anything to draw: an observation, or a rank carried into the range.
        Q_PROPERTY(bool hasData READ hasData NOTIFY dataUpdated)
        // Whether the unfiltered history has any observation, whatever the range shows.
        Q_PROPERTY(bool historyAvailable READ historyAvailable NOTIFY dataUpdated)
        // Actual observations the range shows (for the PB rank, its improvement points; for the
        // recent rank, its play days).
        Q_PROPERTY(int observationCount READ observationCount NOTIFY dataUpdated)
        // Whether non-selectable carried geometry holds a rank out to the range's edges.
        Q_PROPERTY(bool hasCarriedState READ hasCarriedState NOTIFY dataUpdated)
        // History exists but the range shows no observation; Points only then draws nothing.
        Q_PROPERTY(bool noObservationsInRange READ noObservationsInRange NOTIFY dataUpdated)
        Q_PROPERTY(QString emptyStateText READ emptyStateText NOTIFY dataUpdated)
        // Points only over a range whose rank is carried but has no point of its own: nothing is drawn.
        Q_PROPERTY(bool noPointsInRange READ noPointsInRange NOTIFY contentStateChanged)
        // Says why noPointsInRange draws nothing, in this metric's terms.
        Q_PROPERTY(QString noPointsText READ noPointsText CONSTANT)
        // Names the playtime metric's effective averaging window; the rank metric's name never changes.
        Q_PROPERTY(QString metricName READ metricName NOTIFY metricNameChanged)
        // Whether xAxis() is the run axis: X values are then benchmark run numbers, not epoch ms.
        Q_PROPERTY(bool runAxisShown READ runAxisShown NOTIFY runAxisShownChanged)

    public:
        enum Metric { AverageRank, RecentAverageRank, RollingPlaytime };
        Q_ENUM(Metric)

        // Date is axis-only; Value is the single drawn series (mirrors PlaytimeGraphViewModel).
        enum Column { Date = 0, Value = 1 };
        Q_ENUM(Column)

        explicit BenchmarkHistoryViewModel(Metric metric, QObject *parent = nullptr);

        [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override;
        [[nodiscard]] const Axis &xAxis() const override {
            return m_runAxisShown ? static_cast<const Axis &>(m_runAxis) : m_calendarAxis;
        }
        [[nodiscard]] DateTimeAxis &calendarAxis() { return m_calendarAxis; }
        [[nodiscard]] RunDateAxis &runAxis() { return m_runAxis; }
        [[nodiscard]] bool runAxisShown() const { return m_runAxisShown; }
        [[nodiscard]] int yAxisColumn() const override { return Value; }

        [[nodiscard]] bool hasData() const { return m_hasData; }
        [[nodiscard]] bool historyAvailable() const { return m_historyAvailable; }
        [[nodiscard]] int observationCount() const { return m_observationCount; }
        [[nodiscard]] bool hasCarriedState() const { return m_hasCarriedState; }
        [[nodiscard]] bool noObservationsInRange() const { return m_historyAvailable && m_observationCount == 0; }
        [[nodiscard]] QString emptyStateText() const {
            return m_historyAvailable ? tr("No history in the selected range.") : tr("No history recorded yet.");
        }
        [[nodiscard]] bool noPointsInRange() const {
            return m_hasCarriedState && m_observationCount == 0
                   && connectionMode() == static_cast<int>(application::ConnectionMode::PointsOnly);
        }
        [[nodiscard]] QString noPointsText() const;
        [[nodiscard]] QString metricName() const;

        // Only meaningful for RollingPlaytime; takes effect on the next update().
        void setAveragingWindowDays(int days);

        // Takes the "playtime" policy, in minutes, for RollingPlaytime and is ignored for both rank
        // metrics: the rank axis is always tier-based. Kept across updates.
        void setAxisPolicies(const AxisPolicyMap &policies);

        // This metric's graph style: connection, markers, series colour and width, and the playtime
        // axis policy. Kept across updates; the series name never depends on it.
        void setPresentation(const application::GraphPresentation &presentation);
        [[nodiscard]] int averagingWindowDays() const { return m_windowDays; }

        // X is UTC-midnight epoch ms on the calendar axis and the benchmark run number on the run axis;
        // Y is the metric's raw value (rank position, or playtime seconds).
        struct Content {
            QList<QPointF> observations;
            std::optional<QPointF> leadingCarry;
            std::optional<QPointF> trailingCarry;
            // Highest value of the unfiltered history: tier references keep their full-history
            // meaning rather than fitting only what a narrowed range shows.
            qreal fullHistoryPeak = 0.0;
            bool historyAvailable = false;
            // Which axis the content is laid out for; update() shows that axis.
            bool onRunAxis = false;
        };

        // Called by BenchmarkTrackingViewModel inside one atomic rebuild, after it has set the range of
        // the axis `content` is laid out for; this call switches xAxis() to that axis, so the axis and
        // the content laid out on it are always installed together.
        // `present` is false when the metric is suppressed; `rankTiers` is only read
        // for a rank metric: it sizes the rank axis from 0 up to one tier above the full-history peak
        // (capped at the top tier), with a tick at every shown tier, and gives each shown tier a
        // reference line in its colour. The rank line itself has no colour of its own (neutral), so
        // it is never mistaken for a tier line.
        void update(const Content &content, bool present, const std::vector<RankTier> &rankTiers);

    signals:
        void metricNameChanged();
        void contentStateChanged();
        void runAxisShownChanged();

    private:
        [[nodiscard]] bool isRank() const { return m_metric != RollingPlaytime; }
        [[nodiscard]] application::GraphKey graphKey() const {
            switch (m_metric) {
                case AverageRank: return application::GraphKey::BenchmarkRank;
                case RecentAverageRank: return application::GraphKey::BenchmarkRecentRank;
                case RollingPlaytime: break;
            }
            return application::GraphKey::BenchmarkPlaytime;
        }
        [[nodiscard]] static ValueAxis rankYAxis(const std::vector<RankTier> &tiers);

        Metric m_metric;
        int m_windowDays = application::kDefaultAveragingWindowDays;
        application::GraphPresentation m_presentation;
        application::AxisPolicy m_playtimeAxisPolicy;
        SeriesModel *m_series;
        // Every history metric is at most one point per UTC day on it, so it never ticks in hours.
        DateTimeAxis m_calendarAxis{DateTimeAxis::Options{.wholeDays = true}};
        RunDateAxis m_runAxis;
        bool m_runAxisShown = false;
        bool m_hasData = false;
        bool m_historyAvailable = false;
        int m_observationCount = 0;
        bool m_hasCarriedState = false;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_HISTORY_VM_H
