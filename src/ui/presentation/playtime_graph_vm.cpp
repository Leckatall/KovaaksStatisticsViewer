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
        : GraphViewModelBase(parent), m_useCase(std::move(useCase)) {
        refresh();
    }

    QVariantMap PlaytimeGraphViewModel::axisBounds() const {
        QVariantMap map;
        map[QString::number(Date)] = QPointF(m_xAxis.min(), m_xAxis.max());
        map[QString::number(Playtime)] = QPointF(m_yAxis.min(), m_yAxis.max());
        return map;
    }

    QList<qreal> PlaytimeGraphViewModel::axisTicks(const int column) const {
        if (column == Date) return m_xAxis.ticks();
        if (column == Playtime) return m_yAxis.ticks();
        return {};
    }

    QList<QPointF> PlaytimeGraphViewModel::seriesPoints(const int column) const {
        if (column != Playtime) return {};
        return m_points;
    }

    QString PlaytimeGraphViewModel::columnName(const int column) const {
        switch (column) {
            case Date: return "Date";
            case Playtime: return "Playtime (3-day avg)";
            default: return {};
        }
    }

    QColor PlaytimeGraphViewModel::columnColor(const int column) const {
        if (column == Playtime) return kPlaytimeColor;
        return {};
    }

    QString PlaytimeGraphViewModel::columnKey(const int column) const {
        switch (column) {
            case Date: return "date";
            case Playtime: return "playtime";
            default: return {};
        }
    }

    void PlaytimeGraphViewModel::refresh() {
        const auto rollingPlaytime = m_useCase->get_rolling_playtime(kWindowDays);

        m_points.clear();
        m_points.reserve(int(rollingPlaytime.size()));
        QList<QPointF> rawSecondsPoints;
        rawSecondsPoints.reserve(int(rollingPlaytime.size()));
        for (const auto &[epoch_day, avg_seconds]: rollingPlaytime) {
            const qreal epochMs = epochDayToUtcMs(epoch_day);
            m_points.append(QPointF(epochMs, avg_seconds / 60.0));
            rawSecondsPoints.append(QPointF(epochMs, avg_seconds));
        }

        if (m_points.isEmpty()) {
            const qreal nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
            m_xAxis = AxisModel::forDateTimeRange(nowMs, nowMs);
        } else {
            const qreal xlo = m_points.first().x();
            const qreal xhi = m_points.last().x();
            m_xAxis = AxisModel::forDateTimeRange(xlo, xhi, {.targetTicks = 10});
        }
        m_xAxis = m_xAxis.withDelegate(dateDelegate());

        m_series.name = columnName(Playtime);
        m_series.color = kPlaytimeColor;
        m_series.column = Playtime;
        m_series.transform = ValueTransform::secondsToMinutes(); // Plots raw seconds, presents minutes
        m_series.yAxisOptions = {.baseline = AxisModel::Baseline::Zero};
        m_series.setData(rawSecondsPoints);
        m_yAxis = *m_series.yAxis;

        emit dataUpdated();
        emit boundsChanged();
    }
}
