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

    void expectAdjacentLabelsDistinct(const DateTimeAxis &axis) {
        const auto &ticks = axis.ticks();
        ASSERT_FALSE(ticks.isEmpty());
        for (int i = 1; i < ticks.size(); ++i) {
            EXPECT_NE(axis.formatTick(ticks[i - 1]), axis.formatTick(ticks[i]))
                << "adjacent ticks share the label \"" << axis.formatTick(ticks[i]).toStdString() << "\"";
        }
    }

    TEST(DateTimeAxisTest, WholeDayAxisNeverTicksInHours) {
        // Day-granular data spanning one to three days used to tick every 6 hours ("Aug 25 06:00").
        for (const int lastDay: {13, 14, 16}) {
            DateTimeAxis axis({.wholeDays = true});
            axis.setRange(utc(2026, 8, 13), utc(2026, 8, lastDay));
            for (const qreal tick: axis.ticks()) {
                EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(qint64(tick), QTimeZone::utc()).time(), QTime(0, 0));
                EXPECT_FALSE(axis.formatTick(tick).contains(':'));
            }
            expectAdjacentLabelsDistinct(axis);
        }
    }

    TEST(DateTimeAxisTest, WholeDaySinglePointTicksTheSurroundingDays) {
        DateTimeAxis axis({.wholeDays = true});
        axis.setRange(utc(2026, 8, 13), utc(2026, 8, 13));

        EXPECT_EQ(axis.ticks(), (QList<qreal>{utcMs(QDate(2026, 8, 12)), utcMs(QDate(2026, 8, 13)),
                                               utcMs(QDate(2026, 8, 14))}));
        expectAdjacentLabelsDistinct(axis);
    }

    TEST(DateTimeAxisTest, SubDayLabelsShowTheTimeAndDateOnlyWhereNeeded) {
        DateTimeAxis axis({.targetTicks = 8, .timeZone = QTimeZone::utc()});
        axis.setRange(utc(2026, 8, 13, 9, 0), utc(2026, 8, 14, 9, 0));
        const auto &ticks = axis.ticks();
        ASSERT_GE(ticks.size(), 3);

        // The first tick anchors the date; later ticks are just the time, or the date at midnight.
        EXPECT_TRUE(axis.formatTick(ticks.front()).startsWith("Aug 13"));
        for (int i = 1; i < ticks.size(); ++i) {
            const QDateTime at = QDateTime::fromMSecsSinceEpoch(qint64(ticks[i]), QTimeZone::utc());
            const QString label = axis.formatTick(ticks[i]);
            if (at.time() == QTime(0, 0)) EXPECT_EQ(label, at.toString("MMM d"));
            else EXPECT_EQ(label, at.toString("HH:mm"));
        }
        expectAdjacentLabelsDistinct(axis);
    }

    TEST(DateTimeAxisTest, RangeSweepNeverRepeatsALabel) {
        const QDateTime start = utc(2026, 8, 13, 14, 23);
        for (const qint64 spanMinutes: {0LL, 1LL, 7LL, 60LL, 6 * 60LL, 26 * 60LL, 3 * 24 * 60LL, 40 * 24 * 60LL,
                                        200 * 24 * 60LL, 3 * 365 * 24 * 60LL}) {
            for (const bool wholeDays: {false, true}) {
                DateTimeAxis axis({.wholeDays = wholeDays});
                axis.setRange(start, start.addSecs(60 * spanMinutes));
                SCOPED_TRACE(testing::Message() << "span " << spanMinutes << " min, wholeDays " << wholeDays);
                expectAdjacentLabelsDistinct(axis);
            }
        }
    }

    TEST(DateTimeAxisTest, UtcDateTimeForEpochDayZeroIsTheUnixEpoch) {
        EXPECT_EQ(utcDateTimeForEpochDay(0), QDateTime(QDate(1970, 1, 1), QTime(0, 0), QTimeZone::utc()));
    }
}
