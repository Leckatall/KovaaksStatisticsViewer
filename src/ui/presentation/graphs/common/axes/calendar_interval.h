#ifndef KOVAAKSSTATSVIEWER_CALENDAR_INTERVAL_H
#define KOVAAKSSTATSVIEWER_CALENDAR_INTERVAL_H

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTimeZone>
#include <QtGlobal>

#include <array>

namespace ksv::presentation {
    enum class CalendarUnit { Minute, Hour, Day, Week, Month, Year };

    struct CalendarInterval {
        CalendarUnit unit;
        int amount;
        qint64 approximateMs;
    };

    // Ordered by length.
    inline constexpr std::array kCalendarIntervals{
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

    [[nodiscard]] CalendarInterval calendarIntervalFor(qint64 spanMs, int targetTicks, bool wholeDays);

    [[nodiscard]] QDateTime startOfDay(const QDate &date, const QTimeZone &zone);

    // The start of the interval period `value` falls in, in `zone`.
    [[nodiscard]] QDateTime floorDateTime(const QDateTime &value, CalendarInterval interval, const QTimeZone &zone);

    [[nodiscard]] QDateTime advanceDateTime(const QDateTime &value, CalendarInterval interval);

    // `firstTickMs` is the axis' first tick: sub-day labels show only the time, so the first
    // tick (and every midnight) also names the date to anchor the times that follow it.
    [[nodiscard]] QString formatForInterval(CalendarUnit unit, qreal value, const QTimeZone &zone, qint64 firstTickMs);
}

#endif // KOVAAKSSTATSVIEWER_CALENDAR_INTERVAL_H
