#include "presentation/benchmarks/tracking/benchmark_history_vm.h"

#include <QColor>
#include <QString>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <variant>

#include "presentation/benchmarks/benchmark_format.h"
#include "presentation/graphs/playtime_graph_vm.h"
#include "presentation/graphs/common/series/series_style.h"
#include "presentation/graphs/common/axes/value_delegate.h"

namespace ksv::presentation {
    namespace {
        constexpr qreal kEps = 1e-6;

        struct RankBounds {
            qreal bottom;
            qreal top;
            qreal step;
        };

        // The finest of 10, 20 or 50 % of a tier that keeps to about two intervals per tier in view and
        // at least five in all. Each divides a whole tier, so every tier boundary in range is a tick.
        qreal paddedRankStep(const qreal span) {
            const qreal intervals = std::max(5.0, std::ceil(2.0 * span));
            for (const qreal step: {0.1, 0.2}) if (step * intervals >= span - kEps) return step;
            return 0.5;
        }

        // A tier-rounded edge rounds onto the tier it reaches, so a rank held exactly on a tier sits on
        // that tier's line; a padded edge moves strictly outward to the next tick.
        RankBounds fittedRankBounds(const qreal lo, const qreal hi, const qreal tierCount,
                                    const application::TierRounding rounding) {
            using application::TierRounding;
            const bool roundBelow = rounding == TierRounding::Both || rounding == TierRounding::Below;
            const bool roundAbove = rounding == TierRounding::Both || rounding == TierRounding::Above;
            const qreal span = hi - lo < kEps ? 1.0 : hi - lo;
            const qreal step = rounding == TierRounding::Both ? 1.0 : paddedRankStep(span);

            qreal bottom = roundBelow ? std::floor(lo + kEps) : (std::ceil(lo / step - kEps) - 1.0) * step;
            qreal top = roundAbove ? std::ceil(hi - kEps) : (std::floor(hi / step + kEps) + 1.0) * step;
            if (top - bottom < kEps) {
                if (roundBelow) bottom -= 1.0;
                if (roundAbove) top += 1.0;
            }
            bottom = std::max(bottom, 0.0);
            top = std::min(top, tierCount);
            if (top - bottom < kEps) top = bottom + step;
            return {bottom, top, step};
        }

        // Whole tiers read as tier names. Between them (only when the step is finer than a tier) the
        // first tick of each tier band on the axis names the tier it is progress toward and the rest
        // are bare percentages.
        ValueDelegate rankAxisDelegate(std::vector<QString> tierNames, const RankBounds &bounds) {
            ValueDelegate delegate;
            delegate.axisFormatter = [tierNames = std::move(tierNames), step = bounds.step,
                                      bottom = bounds.bottom](const qreal value) -> QString {
                const qreal whole = std::round(value);
                const auto tierCount = static_cast<qreal>(tierNames.size());
                if (std::abs(value - whole) < kEps && whole >= 0.0 && whole <= tierCount) {
                    if (whole == 0.0) return QObject::tr("Unranked");
                    return tierNames[static_cast<std::size_t>(whole) - 1];
                }
                const qreal band = std::floor(value);
                if (step >= 1.0 || band < 0.0 || band >= tierCount) return ValueDelegate().axisLabel(value);
                const int percent = static_cast<int>(std::round((value - band) * 100.0));
                const qreal previous = value - step;
                if (previous <= band + kEps || previous < bottom - kEps)
                    return QObject::tr("%1% to %2").arg(percent).arg(tierNames[static_cast<std::size_t>(band)]);
                return QObject::tr("%1%").arg(percent);
            };
            return delegate;
        }
    }

    BenchmarkHistoryViewModel::BenchmarkHistoryViewModel(const Metric metric, QObject *parent)
        : GraphViewModelBase(parent), m_metric(metric),
          m_playtimeAxisPolicy(axisPolicyFor({}, application::GraphKey::BenchmarkPlaytime, "playtime")),
          m_rankAxisPolicy(axisPolicyFor({}, graphKey(), "rank")),
          m_series(new SeriesModel(this)) {
        setPresentation(application::defaultGraphPresentation(graphKey()));
    }

    void BenchmarkHistoryViewModel::setPresentation(const application::GraphPresentation &presentation) {
        if (presentation == m_presentation) return;
        m_presentation = presentation;
        const bool modeChanged = connectionMode() != static_cast<int>(presentation.appearance.connection);
        setAppearance(presentation.appearance);
        applySeriesStyle(*m_series, presentation, isRank() ? "rank" : "playtime");
        const auto rounding = presentation.tierRounding.value_or(application::TierRounding::Both);
        const bool roundingChanged = isRank() && rounding != m_tierRounding;
        if (isRank()) m_tierRounding = rounding;
        if (!applyAxisPolicies(presentation.axisPolicies) && roundingChanged) refreshRankAxis();
        emit styleChanged();
        if (modeChanged) emit contentStateChanged();
    }

    QString BenchmarkHistoryViewModel::metricName() const {
        switch (m_metric) {
            case AverageRank: return tr("Personal-best average rank");
            case RecentAverageRank: return tr("Recent average rank");
            case RollingPlaytime: break;
        }
        return PlaytimeGraphViewModel::titleFor(m_windowDays);
    }

    QString BenchmarkHistoryViewModel::noPointsText() const {
        if (m_metric == RecentAverageRank)
            return tr("This range has no play days, so Points only draws nothing. "
                      "Choose a line style to see your recent rank held through it.");
        return tr("This range has no improvement points, so Points only draws nothing. "
                  "Choose a line style to see your rank held through it.");
    }

    void BenchmarkHistoryViewModel::setAveragingWindowDays(const int days) {
        if (m_windowDays == days) return;
        m_windowDays = days;
        if (m_metric == RollingPlaytime) emit metricNameChanged();
    }

    QList<SeriesModel *> BenchmarkHistoryViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel *> result;
        if (!m_hasData) return result;
        for (const int column: columns)
            if (column == Value) result.append(m_series);
        return result;
    }

    void BenchmarkHistoryViewModel::setAxisPolicies(const AxisPolicyMap &policies) {
        std::ignore = applyAxisPolicies(policies);
    }

    bool BenchmarkHistoryViewModel::applyAxisPolicies(const AxisPolicyMap &policies) {
        if (isRank()) {
            application::AxisPolicy next = axisPolicyFor(policies, graphKey(), "rank");
            if (next == m_rankAxisPolicy) return false;
            m_rankAxisPolicy = std::move(next);
            refreshRankAxis();
            return true;
        }
        application::AxisPolicy next = axisPolicyFor(policies, application::GraphKey::BenchmarkPlaytime, "playtime");
        if (next == m_playtimeAxisPolicy) return false;
        m_playtimeAxisPolicy = std::move(next);
        if (!m_hasData) return true;
        applyAxisPolicy(*m_series, m_playtimeAxisPolicy);
        emit boundsChanged();
        return true;
    }

    void BenchmarkHistoryViewModel::refreshRankAxis() {
        if (!m_hasData) return;
        m_series->yAxis = rankYAxis();
        emit boundsChanged();
    }

    std::size_t BenchmarkHistoryViewModel::shownTierCount() const {
        // Tiers more than one above the peak rank are out of reach for now; drawing them would squash
        // the line into the bottom of the plot.
        return std::min(m_rankTiers.size(),
                        static_cast<std::size_t>(std::floor(std::max<qreal>(0.0, m_fullHistoryPeak))) + 1);
    }

    ValueAxis BenchmarkHistoryViewModel::rankYAxis() const {
        const auto tierCount = static_cast<qreal>(shownTierCount());
        RankBounds bounds{0.0, tierCount, 1.0};
        if (std::holds_alternative<application::FitVisibleData>(m_rankAxisPolicy))
            if (const auto range = m_series->displayRange())
                bounds = fittedRankBounds(range->first, range->second, tierCount, m_tierRounding);

        std::vector<QString> tierNames;
        tierNames.reserve(m_rankTiers.size());
        for (const auto &tier: m_rankTiers) tierNames.push_back(tier.name);
        ValueAxis::Options options;
        options.niceBounds = false;
        options.distinctLabels = false;
        options.step = bounds.step;
        ValueAxis axis(options, rankAxisDelegate(std::move(tierNames), bounds));
        std::ignore = axis.setRange(bounds.bottom, bounds.top);
        return axis;
    }

    void BenchmarkHistoryViewModel::update(const Content &content, const bool present,
                                           const std::vector<RankTier> &rankTiers) {
        m_historyAvailable = present && content.historyAvailable;
        m_observationCount = present ? static_cast<int>(content.observations.size()) : 0;
        m_hasCarriedState = present && (content.leadingCarry || content.trailingCarry);
        m_hasData = m_observationCount > 0 || m_hasCarriedState;
        if (isRank()) {
            m_rankTiers = rankTiers;
            m_fullHistoryPeak = content.fullHistoryPeak;
        }
        if (content.onRunAxis != m_runAxisShown) {
            m_runAxisShown = content.onRunAxis;
            emit runAxisShownChanged();
        }

        if (!m_hasData) {
            m_series->points.clear();
            m_series->leadingCarry.reset();
            m_series->trailingCarry.reset();
            m_series->referenceLines.clear();
            emit dataUpdated();
            emit boundsChanged();
            emit contentStateChanged();
            return;
        }

        m_series->setId(QString::number(Value));
        m_series->setColumn(Value);
        m_series->leadingCarry = content.leadingCarry;
        m_series->trailingCarry = content.trailingCarry;

        if (isRank()) {
            m_series->setName(metricName());
            // The hover tooltip reads the rank as a tier and progress, not as a bare number.
            m_series->delegate = ValueDelegate::identity();
            std::vector<QString> tierNames;
            tierNames.reserve(rankTiers.size());
            for (const auto &tier: rankTiers) tierNames.push_back(tier.name);
            m_series->delegate.axisFormatter = [tierNames](const qreal value) {
                return describeAverageRank(value, tierNames);
            };
            m_series->points = content.observations;
            m_series->yAxis = rankYAxis();
            m_series->referenceLines.clear();
            // Every shown tier keeps its line whatever the axis spans; the canvas skips the ones
            // outside it.
            for (std::size_t index = 0; index < shownTierCount(); ++index)
                m_series->referenceLines.append({static_cast<qreal>(index + 1), rankTiers[index].color});
        } else {
            m_series->setName(metricName());
            m_series->delegate = ValueDelegate::secondsToMinutes();
            m_series->points = content.observations;
            applyAxisPolicy(*m_series, m_playtimeAxisPolicy);
        }

        emit dataUpdated();
        emit boundsChanged();
        emit contentStateChanged();
    }
}
