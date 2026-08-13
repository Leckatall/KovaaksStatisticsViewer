//
// Created by Lecka on 08/08/2026.
//

#include "axis_model.h"

#include <QDateTime>
#include <QTimeZone>

#include <algorithm>
#include <array>
#include <cmath>

namespace ksv::presentation {
    namespace {
        // Heckbert's "nice number" rounding. Returns a value close to `range`
        qreal niceNum(const qreal range, const bool round) {
            if (range <= 0.0) return 1.0;
            const qreal exponent = std::floor(std::log10(range));
            const qreal fraction = range / std::pow(10.0, exponent);

            qreal niceFraction;
            if (round) {
                if (fraction < 1.5) niceFraction = 1.0;
                else if (fraction < 3.0) niceFraction = 2.0;
                else if (fraction < 7.0) niceFraction = 5.0;
                else niceFraction = 10.0;
            } else {
                if (fraction <= 1.0) niceFraction = 1.0;
                else if (fraction <= 2.0) niceFraction = 2.0;
                else if (fraction <= 5.0) niceFraction = 5.0;
                else niceFraction = 10.0;
            }
            return niceFraction * std::pow(10.0, exponent);
        }

        enum class CalendarUnit { Minute, Hour, Day, Week, Month, Year };

        struct CalendarInterval {
            CalendarUnit unit;
            int amount;
            qint64 approximateMs;
        };

        constexpr std::array kCalendarIntervals{
                CalendarInterval{CalendarUnit::Minute, 1, 60'000LL},
                CalendarInterval{CalendarUnit::Minute, 5, 5 * 60'000LL},
                CalendarInterval{CalendarUnit::Minute, 15, 15 * 60'000LL},
                CalendarInterval{CalendarUnit::Hour, 1, 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Hour, 3, 3 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Hour, 6, 6 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Day, 1, 24 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Week, 1, 7 * 24 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Week, 2, 14 * 24 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Month, 1, 30 * 24 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Month, 3, 90 * 24 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Month, 6, 180 * 24 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Year, 1, 365 * 24 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Year, 2, 2 * 365 * 24 * 60 * 60'000LL},
                CalendarInterval{CalendarUnit::Year, 5, 5 * 365 * 24 * 60 * 60'000LL},
        };

        CalendarInterval calendarIntervalFor(const qint64 spanMs, const int targetTicks) {
            const double target = std::max(1, targetTicks);
            return *std::min_element(kCalendarIntervals.begin(), kCalendarIntervals.end(),
                                     [spanMs, target](const CalendarInterval &left, const CalendarInterval &right) {
                                         return std::abs(double(spanMs) / left.approximateMs - target) <
                                                std::abs(double(spanMs) / right.approximateMs - target);
                                     });
        }

        QDateTime startOfDay(const QDate &date) {
            return QDateTime(date, QTime(0, 0), QTimeZone::utc());
        }

        QDateTime floorDateTime(const QDateTime &value, const CalendarInterval interval) {
            const QDate date = value.date();
            const QTime time = value.time();
            switch (interval.unit) {
                case CalendarUnit::Minute:
                    return QDateTime(date, QTime(time.hour(), time.minute() / interval.amount * interval.amount),
                                     QTimeZone::utc());
                case CalendarUnit::Hour:
                    return QDateTime(date, QTime(time.hour() / interval.amount * interval.amount, 0), QTimeZone::utc());
                case CalendarUnit::Day: return startOfDay(date);
                case CalendarUnit::Week: return startOfDay(date.addDays(1 - date.dayOfWeek()));
                case CalendarUnit::Month:
                    return startOfDay(QDate(date.year(), (date.month() - 1) / interval.amount * interval.amount + 1, 1));
                case CalendarUnit::Year: return startOfDay(QDate(date.year() / interval.amount * interval.amount, 1, 1));
            }
            return value;
        }

        QDateTime advanceDateTime(const QDateTime &value, const CalendarInterval interval) {
            switch (interval.unit) {
                case CalendarUnit::Minute: return value.addSecs(60 * interval.amount);
                case CalendarUnit::Hour: return value.addSecs(60 * 60 * interval.amount);
                case CalendarUnit::Day: return value.addDays(interval.amount);
                case CalendarUnit::Week: return value.addDays(7 * interval.amount);
                case CalendarUnit::Month: return value.addMonths(interval.amount);
                case CalendarUnit::Year: return value.addYears(interval.amount);
            }
            return value;
        }
    }

    AxisModel AxisModel::forRange(const qreal dataLo, const qreal dataHi, const Options options) {
        qreal lo = options.baseline == Baseline::Zero ? 0.0 : dataLo;
        qreal hi = dataHi;

        // Widen a degenerate/empty range so the axis is never zero-width.
        if (hi <= lo) {
            if (options.baseline == Baseline::Zero) {
                hi = 2.0 * options.fallbackSpan;
            } else {
                const qreal centre = lo;
                lo = centre - options.fallbackSpan;
                hi = centre + options.fallbackSpan;
            }
        }

        const int intervals = std::max(1, options.targetTicks);
        qreal step = niceNum(niceNum(hi - lo, false) / intervals, true);
        if (options.integral) step = std::max(1.0, std::round(step));

        // Epsilon handles non-representable values (e.g. 0.05) that round incorrectly
        constexpr qreal kEps = 1e-9;
        qreal niceMin = std::floor(lo / step + kEps) * step;
        qreal niceMax = std::ceil(hi / step - kEps) * step;
        if (options.baseline == Baseline::Zero) niceMin = 0.0;

        AxisModel axis;
        axis.m_min = niceMin;
        axis.m_max = niceMax;

        axis.m_ticks.clear();
        // Index from niceMin instead of accumulating step to avoid floating-point drift
        const int tickCount = int(std::round((niceMax - niceMin) / step));
        for (int i = 0; i <= tickCount; ++i) {
            axis.m_ticks.append(niceMin + step * i);
        }

        return axis;
    }

    AxisModel AxisModel::forDateTimeRange(qreal dataLoMs, qreal dataHiMs, const DateTimeOptions options) {
        qint64 loMs = qint64(std::llround(std::min(dataLoMs, dataHiMs)));
        qint64 hiMs = qint64(std::llround(std::max(dataLoMs, dataHiMs)));
        QDateTime lo = QDateTime::fromMSecsSinceEpoch(loMs, QTimeZone::utc());
        QDateTime hi = QDateTime::fromMSecsSinceEpoch(hiMs, QTimeZone::utc());

        if (loMs == hiMs) {
            lo = startOfDay(lo.date()).addDays(-1);
            hi = startOfDay(hi.date()).addDays(1);
            loMs = lo.toMSecsSinceEpoch();
            hiMs = hi.toMSecsSinceEpoch();
        }

        const CalendarInterval interval = calendarIntervalFor(hiMs - loMs, options.targetTicks);
        const QDateTime min = floorDateTime(lo, interval);
        QDateTime max = floorDateTime(hi, interval);
        if (max < hi) max = advanceDateTime(max, interval);

        AxisModel axis;
        axis.m_min = min.toMSecsSinceEpoch();
        axis.m_max = max.toMSecsSinceEpoch();
        axis.m_ticks.clear();
        for (QDateTime tick = min; tick <= max; tick = advanceDateTime(tick, interval)) {
            axis.m_ticks.append(tick.toMSecsSinceEpoch());
        }
        return axis;
    }
}
