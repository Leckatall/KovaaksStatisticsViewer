//
// Created by Lecka on 08/08/2026.
//

#include "playtime_graph_vm.h"

#include "series_style.h"
#include <QDateTime>
#include <QTimeZone>

#include <algorithm>
#include <tuple>
#include <utility>

namespace ksv::presentation {
    PlaytimeGraphViewModel::PlaytimeGraphViewModel(std::shared_ptr<application::IPlaytimeGraphUseCase> useCase,
                                                   QObject *parent)
        : GraphViewModelBase(parent), m_useCase(std::move(useCase)),
          m_axisPolicy(axisPolicyFor({}, application::GraphKey::DailyPlaytime, "playtime")),
          m_xAxis(DateTimeAxis::Options{.wholeDays = true}), m_series(new SeriesModel(this)) {
        m_useCase->onChanged([this] { refresh(); });
        m_useCase->onPresentationChanged([this] { applyPresentation(); });
        applyPresentation();
        refresh();
    }

    void PlaytimeGraphViewModel::applyPresentation() {
        const auto presentation = m_useCase->presentation();
        setAppearance(presentation.appearance);
        applySeriesStyle(*m_series, presentation, "playtime");
        setAxisPolicies(presentation.axisPolicies);
        emit styleChanged();
    }

    QString PlaytimeGraphViewModel::title() const {
        return m_windowDays == 1 ? tr("Daily Playtime (daily totals)")
                                 : tr("Daily Playtime (%1-day rolling average)").arg(m_windowDays);
    }

    QString PlaytimeGraphViewModel::emptyStateText() const {
        return m_historyAvailable ? tr("No playtime in the selected range.") : tr("No playtime recorded yet.");
    }

    void PlaytimeGraphViewModel::setAxisPolicies(const AxisPolicyMap &policies) {
        application::AxisPolicy next = axisPolicyFor(policies, application::GraphKey::DailyPlaytime, "playtime");
        if (next == m_axisPolicy) return;
        m_axisPolicy = std::move(next);
        applyAxisPolicy(*m_series, m_axisPolicy);
        emit boundsChanged();
    }

    void PlaytimeGraphViewModel::refresh() {
        const auto snapshot = m_useCase->snapshot();

        QList<QPointF> rawSecondsPoints;
        rawSecondsPoints.reserve(int(snapshot.samples.size()));
        for (const auto &[day, avg_seconds]: snapshot.samples) {
            const auto epochMs = static_cast<qreal>(utcMidnightMs(day));
            rawSecondsPoints.append(QPointF(epochMs, avg_seconds));
        }

        if (snapshot.view.interval) {
            const auto &interval = *snapshot.view.interval;
            const auto firstMs = utcMidnightMs(interval.first);
            std::ignore = m_xAxis.setEpochMillisecondsRange(firstMs, std::max(firstMs, utcMidnightMs(interval.last)));
        } else if (rawSecondsPoints.isEmpty()) {
            const QDateTime now = QDateTime::currentDateTimeUtc();
            std::ignore = m_xAxis.setRange(now, now);
        } else {
            const QDateTime lo = QDateTime::fromMSecsSinceEpoch(qint64(rawSecondsPoints.first().x()), QTimeZone::utc());
            const QDateTime hi = QDateTime::fromMSecsSinceEpoch(qint64(rawSecondsPoints.last().x()), QTimeZone::utc());
            std::ignore = m_xAxis.setRange(lo, hi);
        }

        m_historyAvailable = snapshot.view.fullExtent.has_value() || !rawSecondsPoints.isEmpty();

        const bool windowMoved = m_windowDays != snapshot.view.windowDays;
        m_windowDays = snapshot.view.windowDays;

        m_series->setId(QString::number(Playtime));
        m_series->setName(m_windowDays == 1 ? tr("Playtime (daily total)") : tr("Playtime (%1-day avg)").arg(m_windowDays));
        m_series->setColumn(Playtime);
        m_series->transform = ValueTransform::secondsToMinutes(); // Plots raw seconds, presents minutes
        m_series->points = rawSecondsPoints;
        applyAxisPolicy(*m_series, m_axisPolicy);
        if (windowMoved) emit windowChanged();
        emit dataUpdated();
        emit boundsChanged();
    }
}
