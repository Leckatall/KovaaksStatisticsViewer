#ifndef KOVAAKSSTATSVIEWER_DATE_TIME_AXIS_H
#define KOVAAKSSTATSVIEWER_DATE_TIME_AXIS_H

#include <QDateTime>
#include <QTimeZone>
#include <QtGlobal>

#include "axis.h"

namespace ksv::presentation {
    // Persistent calendar-aware axis: selects minute/hour/day/week/month/year tick spacing from the
    // configured range and labels ticks accordingly. Named epoch-millisecond overload prevents an
    // unlabelled qreal overload from obscuring units.
    class DateTimeAxis final : public Axis {
    public:
        struct Options {
            int targetTicks = 10;
            QTimeZone timeZone = QTimeZone::utc();
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
}

#endif //KOVAAKSSTATSVIEWER_DATE_TIME_AXIS_H
