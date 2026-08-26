//
// Created by Lecka on 08/08/2026.
//

#include "playtime_graph_vm.h"

#include <QDateTime>
#include <QTimeZone>
#include <algorithm>
#include <utility>

namespace ksv::presentation {
    namespace {
        const QColor kPlaytimeColor("#4DD0E1");

        qint64 epochDayToUtcMs(const long long epochDay) {
            return QDateTime(QDate(1970, 1, 1).addDays(epochDay), QTime(0, 0), QTimeZone::utc()).toMSecsSinceEpoch();
        }

        QString formatEpochMs(const qreal value) {
            return QDateTime::fromMSecsSinceEpoch(qint64(std::llround(value)), QTimeZone::utc()).date().toString("MMM d");
        }

        ValueTransform dateDelegate() {
            ValueTransform t;
            t.formatter = &formatEpochMs;
            return t;
        }
    }

    PlaytimeGraphViewModel::PlaytimeGraphViewModel(std::shared_ptr<application::IPlaytimeGraphUseCase> useCase,
                                                   QObject *parent)
        : GraphViewModelBase(parent), m_useCase(std::move(useCase)), m_series(new SeriesModel(this)) {
        refresh();
    }

    void PlaytimeGraphViewModel::refresh() {
        const auto rollingPlaytime = m_useCase->get_rolling_playtime(kWindowDays);

        QList<QPointF> rawSecondsPoints;
        rawSecondsPoints.reserve(int(rollingPlaytime.size()));
        for (const auto &[epoch_day, avg_seconds]: rollingPlaytime) {
            const qreal epochMs = epochDayToUtcMs(epoch_day);
            rawSecondsPoints.append(QPointF(epochMs, avg_seconds));
        }

        if (rawSecondsPoints.isEmpty()) {
            const qreal nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
            m_xAxis = AxisModel::forDateTimeRange(nowMs, nowMs);
        } else {
            const qreal xlo = rawSecondsPoints.first().x();
            const qreal xhi = rawSecondsPoints.last().x();
            m_xAxis = AxisModel::forDateTimeRange(xlo, xhi, {.targetTicks = 10});
        }
        m_xAxis = m_xAxis.withDelegate(dateDelegate());

        m_series->setId(QString::number(Playtime));
        m_series->setName("Playtime (3-day avg)");
        m_series->setColor(kPlaytimeColor);
        m_series->setColumn(Playtime);
        m_series->transform = ValueTransform::secondsToMinutes(); // Plots raw seconds, presents minutes
        m_series->yAxisOptions = {.baseline = AxisModel::Baseline::Zero};
        m_series->setData(rawSecondsPoints);
        emit dataUpdated();
        emit boundsChanged();
    }
}
