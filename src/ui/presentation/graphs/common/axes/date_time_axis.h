#ifndef KOVAAKSSTATSVIEWER_DATE_TIME_AXIS_H
#define KOVAAKSSTATSVIEWER_DATE_TIME_AXIS_H

#include <QDateTime>
#include <QTimeZone>
#include <QtGlobal>

#include <chrono>

#include "presentation/graphs/common/axes/axis.h"

namespace ksv::presentation {
    // Persistent calendar-aware axis: selects minute/hour/day/week/month/year tick spacing from the
    // configured range and labels ticks accordingly. Sub-day ticks are labelled with the time only
    // ("HH:mm"), except the first tick and midnights, which carry the date, so labels stay short.
    // Named epoch-millisecond overload prevents an unlabelled qreal overload from obscuring units.
    class DateTimeAxis final : public Axis {
    public:
        struct Options {
            int targetTicks = 10;
            QTimeZone timeZone = QTimeZone::utc();
            // Day-granular data (one point per calendar day): never pick a sub-day tick interval,
            // so the axis is labelled with dates rather than hours that carry no data.
            bool wholeDays = false;
        };

        // Default arguments referring to Options (a nested type of this class) trigger a GCC 11
        // MinGW bug when written inline (`Options options = {}`); this overload delegates instead.
        DateTimeAxis();
        explicit DateTimeAxis(Options options);

        [[nodiscard]] bool setRange(const QDateTime &dataLo, const QDateTime &dataHi);
        [[nodiscard]] bool setEpochMillisecondsRange(qint64 dataLoMs, qint64 dataHiMs);
        [[nodiscard]] bool setOptions(Options options);
        [[nodiscard]] const Options &options() const { return m_options; }

    private:
        [[nodiscard]] bool recompute();

        Options m_options;
        QDateTime m_dataLo;
        QDateTime m_dataHi;
    };

    // Days since the Unix epoch -> UTC-midnight QDateTime, the X unit every calendar-day graph plots in.
    [[nodiscard]] QDateTime utcDateTimeForEpochDay(long long epochDay);

    [[nodiscard]] qint64 utcMidnightMs(std::chrono::sys_days day);
}

#endif //KOVAAKSSTATSVIEWER_DATE_TIME_AXIS_H
