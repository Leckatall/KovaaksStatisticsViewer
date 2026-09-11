#include "date_time_axis.h"

#include <QDate>
#include <QTime>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ksv::presentation {
    namespace {
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

        QDateTime startOfDay(const QDate &date, const QTimeZone &zone) {
            return QDateTime(date, QTime(0, 0), zone);
        }

        QDateTime floorDateTime(const QDateTime &value, const CalendarInterval interval, const QTimeZone &zone) {
            const QDate date = value.date();
            const QTime time = value.time();
            switch (interval.unit) {
                case CalendarUnit::Minute:
                    return QDateTime(date, QTime(time.hour(), time.minute() / interval.amount * interval.amount),
                                     zone);
                case CalendarUnit::Hour:
                    return QDateTime(date, QTime(time.hour() / interval.amount * interval.amount, 0), zone);
                case CalendarUnit::Day: return startOfDay(date, zone);
                case CalendarUnit::Week: return startOfDay(date.addDays(1 - date.dayOfWeek()), zone);
                case CalendarUnit::Month:
                    return startOfDay(QDate(date.year(), (date.month() - 1) / interval.amount * interval.amount + 1, 1),
                                       zone);
                case CalendarUnit::Year: return startOfDay(QDate(date.year() / interval.amount * interval.amount, 1, 1),
                                                            zone);
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

        QString formatForInterval(const CalendarUnit unit, const qreal value, const QTimeZone &zone) {
            const QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(qint64(std::llround(value)), zone);
            switch (unit) {
                case CalendarUnit::Minute:
                case CalendarUnit::Hour: return dateTime.toString(QStringLiteral("MMM d HH:mm"));
                case CalendarUnit::Day:
                case CalendarUnit::Week: return dateTime.toString(QStringLiteral("MMM d"));
                case CalendarUnit::Month: return dateTime.toString(QStringLiteral("MMM yyyy"));
                case CalendarUnit::Year: return dateTime.toString(QStringLiteral("yyyy"));
            }
            return dateTime.toString(QStringLiteral("MMM d"));
        }
    }

    DateTimeAxis::DateTimeAxis() : DateTimeAxis(Options{}) {}

    DateTimeAxis::DateTimeAxis(const Options options)
        : m_options(options), m_dataLo(QDateTime::currentDateTimeUtc()), m_dataHi(m_dataLo) {
        recompute();
    }

    bool DateTimeAxis::setRange(const QDateTime &dataLo, const QDateTime &dataHi) {
        if (!dataLo.isValid() || !dataHi.isValid()) {
            Q_ASSERT(false && "DateTimeAxis::setRange requires valid QDateTime endpoints");
            return false;
        }
        m_dataLo = dataLo < dataHi ? dataLo : dataHi;
        m_dataHi = dataLo < dataHi ? dataHi : dataLo;
        return recompute();
    }

    bool DateTimeAxis::setEpochMillisecondsRange(const qint64 dataLoMs, const qint64 dataHiMs) {
        return setRange(QDateTime::fromMSecsSinceEpoch(dataLoMs, m_options.timeZone),
                        QDateTime::fromMSecsSinceEpoch(dataHiMs, m_options.timeZone));
    }

    bool DateTimeAxis::setOptions(const Options options) {
        m_options = options;
        return recompute();
    }

    bool DateTimeAxis::recompute() {
        const QTimeZone &zone = m_options.timeZone;
        QDateTime lo = m_dataLo.toTimeZone(zone);
        QDateTime hi = m_dataHi.toTimeZone(zone);

        if (lo == hi) {
            lo = startOfDay(lo.date(), zone).addDays(-1);
            hi = startOfDay(hi.date(), zone).addDays(1);
        }

        const CalendarInterval interval = calendarIntervalFor(lo.msecsTo(hi), m_options.targetTicks);
        const QDateTime min = floorDateTime(lo, interval, zone);
        QDateTime max = floorDateTime(hi, interval, zone);
        if (max < hi) max = advanceDateTime(max, interval);

        QList<qreal> ticks;
        for (QDateTime tick = min; tick <= max; tick = advanceDateTime(tick, interval))
            ticks.append(static_cast<qreal>(tick.toMSecsSinceEpoch()));

        ValueTransform transform;
        const CalendarUnit unit = interval.unit;
        transform.formatter = [unit, zone](const qreal value) { return formatForInterval(unit, value, zone); };

        return replaceResolvedState(static_cast<qreal>(min.toMSecsSinceEpoch()),
                                    static_cast<qreal>(max.toMSecsSinceEpoch()), std::move(ticks),
                                    std::move(transform));
    }

    QDateTime utcDateTimeForEpochDay(const long long epochDay) {
        return QDateTime(QDate(1970, 1, 1).addDays(epochDay), QTime(0, 0), QTimeZone::utc());
    }
}
