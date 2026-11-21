#include "presentation/graphs/common/axes/date_time_axis.h"

#include <QDate>
#include <QTime>

#include <utility>

#include "presentation/graphs/common/axes/calendar_interval.h"

namespace ksv::presentation {
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
        m_hasRange = true;
        return recompute();
    }

    std::optional<std::pair<qint64, qint64>> DateTimeAxis::calendarRangeMs() const {
        if (!m_hasRange) return std::nullopt;
        return std::pair{m_dataLo.toMSecsSinceEpoch(), m_dataHi.toMSecsSinceEpoch()};
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

        const CalendarInterval interval = calendarIntervalFor(lo.msecsTo(hi), m_options.targetTicks,
                                                              m_options.wholeDays);
        QDateTime firstTick = floorDateTime(lo, interval, zone);
        if (firstTick < lo) firstTick = advanceDateTime(firstTick, interval);

        QList<qreal> ticks;
        for (QDateTime tick = firstTick; tick <= hi; tick = advanceDateTime(tick, interval))
            ticks.append(static_cast<qreal>(tick.toMSecsSinceEpoch()));
        if (ticks.isEmpty()) ticks.append(static_cast<qreal>(lo.toMSecsSinceEpoch()));

        ValueDelegate delegate;
        const CalendarUnit unit = interval.unit;
        const qint64 firstTickMs = qint64(ticks.front());
        delegate.axisFormatter = [unit, zone, firstTickMs](const qreal value) {
            return formatForInterval(unit, value, zone, firstTickMs);
        };

        return replaceResolvedState(static_cast<qreal>(lo.toMSecsSinceEpoch()),
                                    static_cast<qreal>(hi.toMSecsSinceEpoch()), std::move(ticks),
                                    std::move(delegate));
    }

    QDateTime utcDateTimeForEpochDay(const long long epochDay) {
        return QDateTime(QDate(1970, 1, 1).addDays(epochDay), QTime(0, 0), QTimeZone::utc());
    }

    qint64 utcMidnightMs(const std::chrono::sys_days day) {
        return utcDateTimeForEpochDay(static_cast<long long>(day.time_since_epoch().count())).toMSecsSinceEpoch();
    }
}
