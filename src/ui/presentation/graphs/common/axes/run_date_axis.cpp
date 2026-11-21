#include "presentation/graphs/common/axes/run_date_axis.h"

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <memory>

#include "presentation/graphs/common/axes/calendar_interval.h"

namespace ksv::presentation {
    namespace {
        QDate utcDate(const qint64 ms) { return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc()).date(); }

        QDateTime periodStart(const QDate &date, const CalendarInterval interval) {
            return floorDateTime(startOfDay(date, QTimeZone::utc()), interval, QTimeZone::utc());
        }

        // Indices of the runs that open a period: the first run, then every run whose period start
        // differs from the run before it. Stops once there are more than `limit`.
        std::vector<int> periodOpeners(const std::vector<QDate> &dates, const CalendarInterval interval,
                                       const std::size_t limit) {
            std::vector<int> openers{0};
            QDate previous = periodStart(dates.front(), interval).date();
            for (std::size_t index = 1; index < dates.size() && openers.size() <= limit; ++index) {
                const QDate start = periodStart(dates[index], interval).date();
                if (start != previous) openers.push_back(static_cast<int>(index));
                previous = start;
            }
            return openers;
        }
    }

    RunDateAxis::RunDateAxis() : RunDateAxis(Options{}) {}

    RunDateAxis::RunDateAxis(const Options options) : m_options(options) {}

    bool RunDateAxis::setRuns(std::vector<qint64> startsMs, const int firstRun,
                              std::optional<std::pair<qint64, qint64>> calendarRange) {
        m_calendarRange = calendarRange;
        if (startsMs.empty()) {
            ValueDelegate empty;
            empty.axisFormatter = [](qreal) { return QString(); };
            return replaceResolvedState(firstRun - 0.5, firstRun + 0.5, {}, std::move(empty));
        }

        const int lastRun = firstRun + static_cast<int>(startsMs.size()) - 1;
        const int target = std::max(1, m_options.targetTicks);
        std::vector<QDate> dates;
        dates.reserve(startsMs.size());
        for (const auto ms: startsMs) dates.push_back(utcDate(ms));

        // The ladder from one day up; the longest is used when even it has too many periods.
        const auto firstDayInterval = std::ranges::find(kCalendarIntervals, CalendarUnit::Day, &CalendarInterval::unit);
        CalendarInterval interval = kCalendarIntervals.back();
        std::vector<int> openers;
        for (auto candidate = firstDayInterval; candidate != kCalendarIntervals.end(); ++candidate) {
            openers = periodOpeners(dates, *candidate, static_cast<std::size_t>(target));
            interval = *candidate;
            if (openers.size() <= static_cast<std::size_t>(target)) break;
        }

        // A pixel gap is not available here: an axis has no width.
        const double minimumGap = static_cast<double>(lastRun - firstRun) / (2.0 * target);
        QList<qreal> ticks;
        int lastKept = 0;
        for (const int opener: openers) {
            if (!ticks.isEmpty() && opener - lastKept < minimumGap) continue;
            ticks.append(firstRun + opener);
            lastKept = opener;
        }

        struct Shared {
            std::vector<QDate> dates;
            int firstRun;
            CalendarInterval interval;
        };
        const auto shared = std::make_shared<const Shared>(Shared{std::move(dates), firstRun, interval});
        const auto nearest = [shared](const qreal value) -> const QDate & {
            const auto index = std::clamp(qRound(value) - shared->firstRun, 0, static_cast<int>(shared->dates.size()) - 1);
            return shared->dates[static_cast<std::size_t>(index)];
        };
        const QString hoverFormat = shared->dates.front().year() != shared->dates.back().year()
                                        ? QStringLiteral("MMM d yyyy")
                                        : QStringLiteral("MMM d");

        ValueDelegate delegate;
        delegate.axisFormatter = [shared, nearest](const qreal value) {
            const auto start = periodStart(nearest(value), shared->interval);
            return formatForInterval(shared->interval.unit, static_cast<qreal>(start.toMSecsSinceEpoch()),
                                     QTimeZone::utc(), start.toMSecsSinceEpoch());
        };
        delegate.hoverFormatter = [nearest, hoverFormat](const qreal value) {
            return nearest(value).toString(hoverFormat);
        };

        const qreal padding = firstRun == lastRun ? 0.5 : 0.0;
        return replaceResolvedState(firstRun - padding, lastRun + padding, std::move(ticks), std::move(delegate));
    }

    std::optional<std::pair<qint64, qint64>> RunDateAxis::calendarRangeMs() const { return m_calendarRange; }
}
