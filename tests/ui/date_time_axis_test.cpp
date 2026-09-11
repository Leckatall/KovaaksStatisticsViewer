//
// DateTimeAxis (calendar-aware bounds + ticks) tests.
//

#include <gtest/gtest.h>

#include <QDateTime>
#include <QTimeZone>

#include "date_time_axis.h"

using ksv::presentation::DateTimeAxis;
using ksv::presentation::utcDateTimeForEpochDay;

namespace {
    QDateTime utc(const int y, const int m, const int d, const int h = 0, const int mi = 0) {
        return QDateTime(QDate(y, m, d), QTime(h, mi), QTimeZone::utc());
    }

    qreal utcMs(const QDate &date, const QTime &time = QTime(0, 0)) {
        return QDateTime(date, time, QTimeZone::utc()).toMSecsSinceEpoch();
    }

    TEST(DateTimeAxisTest, EpochMillisecondsAndDateTimeInputsProduceTheSameAxis) {
        const QDateTime lo(QDate(2026, 8, 3), QTime(0, 0), QTimeZone::utc());
        const QDateTime hi(QDate(2026, 8, 8), QTime(0, 0), QTimeZone::utc());
        DateTimeAxis typed;
        DateTimeAxis epoch;
        typed.setRange(lo, hi);
        epoch.setEpochMillisecondsRange(lo.toMSecsSinceEpoch(), hi.toMSecsSinceEpoch());
        EXPECT_EQ(typed.min(), epoch.min());
        EXPECT_EQ(typed.max(), epoch.max());
        EXPECT_EQ(typed.ticks(), epoch.ticks());
    }

    TEST(DateTimeAxisTest, RepeatedRangeUpdatesKeepTimezoneAndTargetTicks) {
        DateTimeAxis axis({.targetTicks = 4, .timeZone = QTimeZone::utc()});
        axis.setRange(utc(2026, 1, 1), utc(2026, 1, 2));
        axis.setRange(utc(2026, 1, 1), utc(2026, 7, 1));
        EXPECT_GE(axis.ticks().size(), 3);
        EXPECT_LE(axis.ticks().size(), 7);
        for (qreal tick : axis.ticks()) {
            EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(qint64(tick), QTimeZone::utc()).time(), QTime(0, 0));
        }
    }

    TEST(DateTimeAxisTest, DefaultLabelsAdaptToCalendarInterval) {
        DateTimeAxis hours({.targetTicks = 4, .timeZone = QTimeZone::utc()});
        hours.setRange(utc(2026, 8, 3, 9, 0), utc(2026, 8, 3, 15, 0));
        EXPECT_TRUE(hours.formatTick(hours.ticks().front()).contains(':'));

        DateTimeAxis months;
        months.setRange(utc(2026, 1, 15), utc(2026, 12, 20));
        EXPECT_TRUE(months.formatTick(months.ticks().front()).contains("2026"));
    }

    TEST(DateTimeAxisTest, DateTimeAxisUsesUtcMidnightTicksForDailyData) {
        DateTimeAxis axis;
        axis.setRange(utc(2026, 8, 3), utc(2026, 8, 8));

        EXPECT_EQ(axis.ticks(), (QList<qreal>{utcMs(QDate(2026, 8, 3)), utcMs(QDate(2026, 8, 4)),
                                               utcMs(QDate(2026, 8, 5)), utcMs(QDate(2026, 8, 6)),
                                               utcMs(QDate(2026, 8, 7)), utcMs(QDate(2026, 8, 8))}));
    }

    TEST(DateTimeAxisTest, DateTimeAxisRoundsLongRangesToMonthBoundaries) {
        DateTimeAxis axis;
        axis.setRange(utc(2026, 1, 15), utc(2026, 12, 20));

        EXPECT_EQ(axis.min(), utcMs(QDate(2026, 1, 1)));
        EXPECT_EQ(axis.max(), utcMs(QDate(2027, 1, 1)));
        for (const qreal tick: axis.ticks()) {
            EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(qint64(tick), QTimeZone::utc()).date().day(), 1);
        }
    }

    TEST(DateTimeAxisTest, DateTimeAxisExpandsASingleInstantAroundItsDate) {
        DateTimeAxis axis;
        axis.setRange(utc(2026, 8, 13, 14, 23), utc(2026, 8, 13, 14, 23));

        EXPECT_EQ(axis.min(), utcMs(QDate(2026, 8, 12)));
        EXPECT_EQ(axis.max(), utcMs(QDate(2026, 8, 14)));
    }

    TEST(DateTimeAxisTest, ReversedEndpointsAreNormalized) {
        DateTimeAxis forward;
        DateTimeAxis reversed;
        forward.setRange(utc(2026, 8, 3), utc(2026, 8, 8));
        reversed.setRange(utc(2026, 8, 8), utc(2026, 8, 3));

        EXPECT_EQ(forward.min(), reversed.min());
        EXPECT_EQ(forward.max(), reversed.max());
        EXPECT_EQ(forward.ticks(), reversed.ticks());
    }

    TEST(DateTimeAxisTest, MinuteAndHourBoundariesAlign) {
        DateTimeAxis minutes({.targetTicks = 4, .timeZone = QTimeZone::utc()});
        minutes.setRange(utc(2026, 8, 3, 9, 2), utc(2026, 8, 3, 9, 12));
        for (const qreal tick: minutes.ticks()) {
            const QTime time = QDateTime::fromMSecsSinceEpoch(qint64(tick), QTimeZone::utc()).time();
            EXPECT_EQ(time.second(), 0);
        }

        DateTimeAxis hours({.targetTicks = 4, .timeZone = QTimeZone::utc()});
        hours.setRange(utc(2026, 8, 3, 1, 10), utc(2026, 8, 3, 19, 40));
        for (const qreal tick: hours.ticks()) {
            const QTime time = QDateTime::fromMSecsSinceEpoch(qint64(tick), QTimeZone::utc()).time();
            EXPECT_EQ(time.minute(), 0);
        }
    }

    TEST(DateTimeAxisTest, WeekBoundariesAlignToTheStartOfTheWeek) {
        DateTimeAxis axis({.targetTicks = 6, .timeZone = QTimeZone::utc()});
        axis.setRange(utc(2026, 1, 1), utc(2026, 3, 15));
        for (const qreal tick: axis.ticks()) {
            const QDate date = QDateTime::fromMSecsSinceEpoch(qint64(tick), QTimeZone::utc()).date();
            if (date.day() != 1) EXPECT_EQ(date.dayOfWeek(), 1);
        }
    }

    TEST(DateTimeAxisTest, YearBoundariesAlignToJanuaryFirst) {
        DateTimeAxis axis;
        axis.setRange(utc(2018, 3, 5), utc(2026, 11, 20));
        for (const qreal tick: axis.ticks()) {
            const QDate date = QDateTime::fromMSecsSinceEpoch(qint64(tick), QTimeZone::utc()).date();
            EXPECT_EQ(date.month(), 1);
            EXPECT_EQ(date.day(), 1);
        }
    }

    TEST(DateTimeAxisTest, UtcDateTimeForEpochDayZeroIsTheUnixEpoch) {
        EXPECT_EQ(utcDateTimeForEpochDay(0), QDateTime(QDate(1970, 1, 1), QTime(0, 0), QTimeZone::utc()));
    }
}
