#include "presentation/graphs/common/axes/calendar_interval.h"

#include <QTime>

#include <algorithm>
#include <cmath>

namespace ksv::presentation {
    namespace {
        constexpr qint64 kDayMs = 24 * 60 * 60'000LL;
    }

    CalendarInterval calendarIntervalFor(const qint64 spanMs, const int targetTicks, const bool wholeDays) {
        const double target = std::max(1, targetTicks);
        // Intervals are ordered by length; whole-day axes start the search at the first day interval.
        const auto first = wholeDays
                               ? std::find_if(kCalendarIntervals.begin(), kCalendarIntervals.end(),
                                              [](const CalendarInterval &interval) {
                                                  return interval.approximateMs >= kDayMs;
                                              })
                               : kCalendarIntervals.begin();
        return *std::min_element(first, kCalendarIntervals.end(),
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

    QString formatForInterval(const CalendarUnit unit, const qreal value, const QTimeZone &zone,
                              const qint64 firstTickMs) {
        const qint64 valueMs = qint64(std::llround(value));
        const QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(valueMs, zone);
        switch (unit) {
            case CalendarUnit::Minute:
            case CalendarUnit::Hour:
                if (valueMs == firstTickMs) {
                    return dateTime.time() == QTime(0, 0)
                               ? dateTime.toString(QStringLiteral("MMM d"))
                               : dateTime.toString(QStringLiteral("MMM d HH:mm"));
                }
                if (dateTime.time() == QTime(0, 0)) return dateTime.toString(QStringLiteral("MMM d"));
                return dateTime.toString(QStringLiteral("HH:mm"));
            case CalendarUnit::Day:
            case CalendarUnit::Week: return dateTime.toString(QStringLiteral("MMM d"));
            case CalendarUnit::Month: return dateTime.toString(QStringLiteral("MMM yyyy"));
            case CalendarUnit::Year: return dateTime.toString(QStringLiteral("yyyy"));
        }
        return dateTime.toString(QStringLiteral("MMM d"));
    }
}
