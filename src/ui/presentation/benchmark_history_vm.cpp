#include "benchmark_history_vm.h"

#include <QColor>
#include <QString>
#include <algorithm>
#include <array>
#include <cmath>

#include "benchmark_format.h"
#include "value_transform.h"

namespace ksv::presentation {
    namespace {
        const QColor kPlaytimeColor("#4DD0E1");
    }

    BenchmarkHistoryViewModel::BenchmarkHistoryViewModel(const Metric metric, const Axis &xAxis, QObject *parent)
        : GraphViewModelBase(parent), m_metric(metric),
          m_playtimeAxisPolicy(axisPolicyFor({}, application::GraphKey::BenchmarkPlaytime, "playtime")),
          m_series(new SeriesModel(this)), m_xAxis(&xAxis) {
    }

    QString BenchmarkHistoryViewModel::metricName() const {
        if (m_metric == AverageRank) return tr("Personal-best average rank");
        // The default window keeps its spelled-out wording.
        if (m_windowDays == 1) return tr("Daily playtime");
        if (m_windowDays == 3) return tr("Three-day average playtime");
        return tr("%1-day average playtime").arg(m_windowDays);
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

    void BenchmarkHistoryViewModel::applyPlaytimeYAxis() {
        const std::array<const SeriesModel *, 1> members{m_series};
        m_series->yAxisOptions = optionsForPolicy(m_playtimeAxisPolicy, {});
        m_series->yAxis = axisForPolicy(members, m_playtimeAxisPolicy, {}, m_series->transform);
    }

    void BenchmarkHistoryViewModel::setAxisPolicies(const AxisPolicyMap &policies) {
        if (m_metric != RollingPlaytime) return;
        application::AxisPolicy next = axisPolicyFor(policies, application::GraphKey::BenchmarkPlaytime, "playtime");
        if (next == m_playtimeAxisPolicy) return;
        m_playtimeAxisPolicy = std::move(next);
        if (!m_hasData) return;
        applyPlaytimeYAxis();
        emit boundsChanged();
    }

    ValueAxis BenchmarkHistoryViewModel::rankYAxis(const std::vector<RankTier> &tiers) {
        std::vector<QString> tierNames;
        tierNames.reserve(tiers.size());
        for (const auto &tier: tiers) tierNames.push_back(tier.name);
        ValueTransform transform;
        transform.formatter = [tierNames](const qreal value) -> QString {
            const qreal rounded = std::round(value);
            if (std::abs(value - rounded) < 1e-6 && rounded >= 0.0 &&
                rounded <= static_cast<qreal>(tierNames.size())) {
                const int index = static_cast<int>(rounded);
                if (index == 0) return QObject::tr("Unranked");
                return tierNames[static_cast<std::size_t>(index - 1)];
            }
            return ValueTransform().format(value);
        };
        ValueAxis::Options options;
        options.baseline = ValueAxis::Baseline::Zero;
        options.integral = true;
        // One tick per tier (and Unranked), so every tier has a level; the axis painter labels
        // as many of them as fit without overlapping. Asking for twice as many intervals as tiers
        // makes the nice step at most 1 (12 intervals over 12 tiers would round up to a step of 2),
        // and `integral` then lifts it to exactly 1.
        options.targetTicks = std::max(1, 2 * static_cast<int>(tierNames.size()));
        ValueAxis axis(options, transform);
        axis.setRange(0.0, static_cast<qreal>(tierNames.size()));
        return axis;
    }

    void BenchmarkHistoryViewModel::update(const Content &content, const bool present,
                                           const std::vector<RankTier> &rankTiers) {
        m_historyAvailable = present && content.historyAvailable;
        m_observationCount = present ? static_cast<int>(content.observations.size()) : 0;
        m_hasCarriedState = present && (content.leadingCarry || content.trailingCarry);
        m_hasData = m_observationCount > 0 || m_hasCarriedState;

        if (!m_hasData) {
            m_series->points.clear();
            m_series->leadingCarry.reset();
            m_series->trailingCarry.reset();
            m_series->referenceLines.clear();
            emit dataUpdated();
            emit boundsChanged();
            return;
        }

        m_series->setId(QString::number(Value));
        m_series->setColumn(Value);
        m_series->leadingCarry = content.leadingCarry;
        m_series->trailingCarry = content.trailingCarry;

        if (m_metric == AverageRank) {
            m_series->setName(tr("Personal-best average rank"));
            m_series->setColor(QColor());
            // The hover tooltip reads the rank as a tier and progress, not as a bare number.
            m_series->transform = ValueTransform::identity();
            std::vector<QString> tierNames;
            tierNames.reserve(rankTiers.size());
            for (const auto &tier: rankTiers) tierNames.push_back(tier.name);
            m_series->transform.formatter = [tierNames](const qreal value) {
                return describeAverageRank(value, tierNames);
            };
            m_series->points = content.observations;
            // Tiers more than one above the peak rank are out of reach for now; drawing them would
            // squash the line into the bottom of the plot.
            const auto shownTierCount = std::min(
                rankTiers.size(), static_cast<std::size_t>(std::floor(std::max<qreal>(0.0, content.fullHistoryPeak))) + 1);
            const std::vector<RankTier> shownTiers(rankTiers.begin(),
                                                   rankTiers.begin() + static_cast<std::ptrdiff_t>(shownTierCount));
            m_series->yAxis = rankYAxis(shownTiers);
            m_series->referenceLines.clear();
            for (std::size_t index = 0; index < shownTiers.size(); ++index)
                m_series->referenceLines.append({static_cast<qreal>(index + 1), shownTiers[index].color});
        } else {
            m_series->setName(metricName());
            m_series->setColor(kPlaytimeColor);
            m_series->transform = ValueTransform::secondsToMinutes();
            m_series->points = content.observations;
            applyPlaytimeYAxis();
        }

        emit dataUpdated();
        emit boundsChanged();
    }
}
