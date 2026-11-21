//
// RunDateAxis: one unit per run, ticks on the first run of each UTC calendar period.
//

#include <gtest/gtest.h>

#include <QDateTime>
#include <QTimeZone>

#include <vector>

#include "presentation/graphs/common/axes/run_date_axis.h"

using ksv::presentation::RunDateAxis;

namespace {
    qint64 utcMs(const int y, const int m, const int d, const int h = 12, const int mi = 0) {
        return QDateTime(QDate(y, m, d), QTime(h, mi), QTimeZone::utc()).toMSecsSinceEpoch();
    }

    // One run every `everyDays` days from the given UTC date.
    std::vector<qint64> spacedRuns(const QDate &from, const int count, const int everyDays) {
        std::vector<qint64> starts;
        for (int i = 0; i < count; ++i)
            starts.push_back(QDateTime(from.addDays(static_cast<qint64>(i) * everyDays), QTime(12, 0), QTimeZone::utc())
                                 .toMSecsSinceEpoch());
        return starts;
    }

    RunDateAxis axisOver(std::vector<qint64> starts, const int firstRun = 1) {
        RunDateAxis axis;
        std::ignore = axis.setRuns(std::move(starts), firstRun, std::nullopt);
        return axis;
    }

    QStringList tickLabels(const RunDateAxis &axis) {
        QStringList labels;
        for (const qreal tick: axis.ticks()) labels.append(axis.formatTick(tick));
        return labels;
    }

    TEST(RunDateAxis, ManyRunsOverFewDaysTickTheFirstRunOfEachDay) {
        std::vector<qint64> starts;
        for (const int day: {4, 5, 6})
            for (const int hour: {1, 8, 15, 22}) starts.push_back(utcMs(2026, 3, day, hour));

        const auto axis = axisOver(starts);

        EXPECT_EQ(axis.ticks(), (QList<qreal>{1, 5, 9}));
        EXPECT_EQ(axis.min(), 1);
        EXPECT_EQ(axis.max(), 12);
    }

    TEST(RunDateAxis, LongerSpansCoarsenToTheShortestPeriodWithinTheTarget) {
        // 30 daily runs from a Monday: 30 days is over the target, 5 weeks is within it.
        const auto weeks = axisOver(spacedRuns(QDate(2026, 3, 2), 30, 1));
        EXPECT_EQ(weeks.ticks(), (QList<qreal>{1, 8, 15, 22, 29}));

        // 18 runs ten days apart over six months: too many days and weeks, six months.
        const auto months = axisOver(spacedRuns(QDate(2026, 1, 1), 18, 10));
        EXPECT_EQ(tickLabels(months),
                  (QStringList{"Jan 2026", "Feb 2026", "Mar 2026", "Apr 2026", "May 2026", "Jun 2026"}));

        // 49 runs sixty days apart over eight years: only years fit.
        const auto years = axisOver(spacedRuns(QDate(2018, 1, 10), 49, 60));
        EXPECT_EQ(tickLabels(years),
                  (QStringList{"2018", "2019", "2020", "2021", "2022", "2023", "2024", "2025"}));
    }

    TEST(RunDateAxis, TheFirstVisibleRunAlwaysHasATick) {
        // Starts on a Wednesday at run 37; weekly ticks would otherwise begin at the next Monday.
        const auto axis = axisOver(spacedRuns(QDate(2026, 3, 4), 28, 1), 37);

        ASSERT_FALSE(axis.ticks().isEmpty());
        EXPECT_EQ(axis.ticks().front(), 37);
        EXPECT_EQ(axis.ticks().at(1), 42);
    }

    // Days with few runs would bunch their ticks and grid lines together.
    TEST(RunDateAxis, ATickTooCloseToThePreviousKeptTickIsDropped) {
        std::vector starts{utcMs(2026, 3, 1), utcMs(2026, 3, 2)};
        for (int hour = 0; hour < 20; ++hour) starts.push_back(utcMs(2026, 3, 3, hour));

        const auto axis = axisOver(starts);

        EXPECT_EQ(axis.ticks(), (QList<qreal>{1, 3}));
    }

    TEST(RunDateAxis, TicksAreLabelledInTheirPeriodsFormat) {
        const auto days = axisOver({utcMs(2026, 3, 4), utcMs(2026, 3, 5)});
        EXPECT_EQ(tickLabels(days), (QStringList{"Mar 4", "Mar 5"}));

        const auto months = axisOver(spacedRuns(QDate(2026, 1, 1), 18, 10));
        EXPECT_EQ(months.formatTick(months.ticks().front()), "Jan 2026");

        const auto years = axisOver(spacedRuns(QDate(2018, 1, 10), 49, 60));
        EXPECT_EQ(years.formatTick(years.ticks().front()), "2018");
    }

    TEST(RunDateAxis, ALoneRunIsPaddedByHalfARun) {
        const auto axis = axisOver({utcMs(2026, 3, 4)}, 7);

        EXPECT_EQ(axis.min(), 6.5);
        EXPECT_EQ(axis.max(), 7.5);
        EXPECT_EQ(axis.ticks(), (QList<qreal>{7}));
    }

    TEST(RunDateAxis, HoverNamesTheNearestRunsUtcDate) {
        // 23:30 UTC is already the next day east of Greenwich: the label must stay on the UTC day.
        const auto axis = axisOver({utcMs(2026, 3, 4, 23, 30), utcMs(2026, 3, 6, 1)});

        EXPECT_EQ(axis.formatHover(1.0), "Mar 4");
        EXPECT_EQ(axis.formatHover(1.4), "Mar 4");
        EXPECT_EQ(axis.formatHover(1.6), "Mar 6");
        EXPECT_EQ(axis.formatHover(2.5), "Mar 6");
    }

    TEST(RunDateAxis, HoverAddsTheYearWhenVisibleRunsSpanTwoYears) {
        const auto axis = axisOver({utcMs(2025, 12, 31), utcMs(2026, 1, 2)});

        EXPECT_EQ(axis.formatHover(1.0), "Dec 31 2025");
        EXPECT_EQ(axis.formatHover(2.0), "Jan 2 2026");
    }

    TEST(RunDateAxis, CalendarRangeIsTheRangeItWasGiven) {
        RunDateAxis axis;
        const std::pair range{utcMs(2026, 3, 1, 0), utcMs(2026, 3, 10, 0)};
        std::ignore = axis.setRuns({utcMs(2026, 3, 4), utcMs(2026, 3, 5)}, 3, range);
        EXPECT_EQ(axis.calendarRangeMs(), std::optional(range));

        std::ignore = axis.setRuns({utcMs(2026, 3, 4)}, 1, std::nullopt);
        EXPECT_FALSE(axis.calendarRangeMs());
    }

    TEST(RunDateAxis, NoRunsLeaveADegenerateAxisWithoutTicksOrLabels) {
        const auto axis = axisOver({});

        EXPECT_TRUE(axis.ticks().isEmpty());
        EXPECT_TRUE(axis.formatTick(axis.min()).isEmpty());
        EXPECT_TRUE(axis.formatHover(axis.min()).isEmpty());
    }

    TEST(RunDateAxis, SetRunsReportsWhetherTheGeometryChanged) {
        RunDateAxis axis;
        EXPECT_TRUE(axis.setRuns({utcMs(2026, 3, 4), utcMs(2026, 3, 5)}, 1, std::nullopt));
        EXPECT_FALSE(axis.setRuns({utcMs(2026, 3, 4), utcMs(2026, 3, 5)}, 1, std::nullopt));
        EXPECT_TRUE(axis.setRuns({utcMs(2026, 3, 4), utcMs(2026, 3, 5)}, 2, std::nullopt));
    }
}
