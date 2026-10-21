//
// ValueAxis (nice-number bounds + ticks) tests.
//

#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

#include "value_axis.h"

using ksv::presentation::Axis;
using ksv::presentation::ValueAxis;
using ksv::presentation::ValueTransform;

namespace {
    // Every tick is an (integer) multiple of the spacing between the first two.
    void expectTicksOnRoundGrid(const Axis &axis) {
        const auto &ticks = axis.ticks();
        ASSERT_GE(ticks.size(), 2);
        const double step = ticks[1] - ticks[0];
        ASSERT_GT(step, 0.0);
        for (const double t: ticks) {
            const double k = t / step;
            EXPECT_NEAR(k, std::round(k), 1e-6) << "tick " << t << " is not a multiple of step " << step;
        }
    }

    TEST(ValueAxisTest, TicksLandOnRoundMultiples) {
        ValueAxis axis;
        axis.setRange(10.0, 30.0);
        EXPECT_DOUBLE_EQ(axis.min(), 10.0);
        EXPECT_DOUBLE_EQ(axis.max(), 30.0);
        EXPECT_DOUBLE_EQ(axis.ticks().front(), 10.0);
        EXPECT_DOUBLE_EQ(axis.ticks().back(), 30.0);
        expectTicksOnRoundGrid(axis);
    }

    TEST(ValueAxisTest, ZeroBaselinePinsMinToZero) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::Zero});
        axis.setRange(12.0, 87.0);
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_GE(axis.max(), 87.0);
        EXPECT_DOUBLE_EQ(axis.ticks().front(), 0.0);
        expectTicksOnRoundGrid(axis);
    }

    TEST(ValueAxisTest, HugDataRoundsMinDownToANiceValue) {
        // Data starts well above zero; HugData keeps detail by rounding the
        // lower bound DOWN to a round value rather than pinning it to zero.
        ValueAxis axis({.baseline = ValueAxis::Baseline::HugData});
        axis.setRange(910.0, 985.0);
        EXPECT_GT(axis.min(), 0.0);
        EXPECT_LE(axis.min(), 910.0);
        EXPECT_GE(axis.max(), 985.0);
        expectTicksOnRoundGrid(axis);
    }

    TEST(ValueAxisTest, FractionalRangeStillProducesRoundTicks) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::HugData});
        axis.setRange(0.5, 0.7);
        EXPECT_DOUBLE_EQ(axis.min(), 0.5);
        EXPECT_DOUBLE_EQ(axis.max(), 0.7);
        EXPECT_NEAR(axis.ticks().front(), 0.5, 1e-9);
        EXPECT_NEAR(axis.ticks().back(), 0.7, 1e-9);
        expectTicksOnRoundGrid(axis);
    }

    TEST(ValueAxisTest, IntegralModeNeverProducesFractionalTicks) {
        // A range that would otherwise pick a sub-1 step (e.g. 0.5) must be
        // forced to a whole-number step so day/second labels never duplicate.
        ValueAxis axis({.baseline = ValueAxis::Baseline::Zero, .integral = true});
        axis.setRange(0.0, 2.0);
        for (const double t: axis.ticks()) {
            EXPECT_DOUBLE_EQ(t, std::round(t)) << "integral axis produced fractional tick " << t;
        }
        const double step = axis.ticks()[1] - axis.ticks()[0];
        EXPECT_GE(step, 1.0);
    }

    TEST(ValueAxisTest, DegenerateRangeExpandsToNonZeroWidth) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::HugData});
        axis.setRange(42.0, 42.0);
        EXPECT_LT(axis.min(), axis.max());
        EXPECT_LE(axis.min(), 42.0);
        EXPECT_GE(axis.max(), 42.0);
        expectTicksOnRoundGrid(axis);
    }

    TEST(ValueAxisTest, ZeroBaselineDegenerateRangeStaysGroundedAtZero) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::Zero});
        axis.setRange(0.0, 0.0);
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_GT(axis.max(), 0.0);
    }

    TEST(ValueAxisTest, TickCountIsCloseToTarget) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::Zero, .integral = false, .targetTicks = 5});
        axis.setRange(0.0, 100.0);
        // Nice-number rounding means the realized count varies a little.
        EXPECT_GE(axis.ticks().size(), 4);
        EXPECT_LE(axis.ticks().size(), 8);
    }

    TEST(ValueAxisTest, AllTicksStayWithinBounds) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::HugData});
        axis.setRange(3.0, 97.0);
        for (const double t: axis.ticks()) {
            EXPECT_GE(t, axis.min() - 1e-9);
            EXPECT_LE(t, axis.max() + 1e-9);
        }
        EXPECT_DOUBLE_EQ(axis.ticks().front(), axis.min());
        EXPECT_DOUBLE_EQ(axis.ticks().back(), axis.max());
    }

    TEST(ValueAxisTest, DefaultConstructedAxisIsUnitRange) {
        const ValueAxis axis;
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_DOUBLE_EQ(axis.max(), 1.0);
        EXPECT_FALSE(axis.ticks().isEmpty());
    }

    TEST(ValueAxisTest, NormalizedPositionAndValueAtRoundTrip) {
        ValueAxis axis;
        axis.setRange(10.0, 30.0);
        for (const double value: {10.0, 15.0, 22.5, 30.0}) {
            const double t = axis.normalizedPosition(value);
            EXPECT_NEAR(axis.valueAt(t), value, 1e-9);
        }
        EXPECT_DOUBLE_EQ(axis.normalizedPosition(10.0), 0.0);
        EXPECT_DOUBLE_EQ(axis.normalizedPosition(30.0), 1.0);
        EXPECT_DOUBLE_EQ(axis.valueAt(0.0), 10.0);
        EXPECT_DOUBLE_EQ(axis.valueAt(1.0), 30.0);
    }

    TEST(ValueAxisTest, FormatTickUsesTheAttachedDelegate) {
        ValueAxis axis({}, ValueTransform::percentage());
        axis.setRange(0.0, 100.0);
        EXPECT_EQ(axis.formatTick(40.0), "40%");
    }

    TEST(ValueAxisTest, SetRangePreservesConfiguredPolicy) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::Zero,
                        .integral = true,
                        .targetTicks = 5,
                        .fallbackSpan = 2.0},
                       ValueTransform::percentage());

        axis.setRange(12.0, 87.0);
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        for (const qreal tick : axis.ticks()) EXPECT_DOUBLE_EQ(tick, std::round(tick));

        axis.setRange(3.0, 9.0);
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_EQ(axis.formatTick(5.0), "5%");
        for (const qreal tick : axis.ticks()) EXPECT_DOUBLE_EQ(tick, std::round(tick));
    }

    TEST(ValueAxisTest, SetRangeReportsWhetherResolvedStateChanged) {
        ValueAxis axis;
        EXPECT_TRUE(axis.setRange(10.0, 30.0));
        EXPECT_FALSE(axis.setRange(10.0, 30.0));
        EXPECT_TRUE(axis.setRange(10.0, 40.0));
    }

    // --- Tick labels at degenerate ranges: adjacent labels must always differ. ---

    void expectAdjacentLabelsDistinct(const Axis &axis) {
        const auto &ticks = axis.ticks();
        ASSERT_FALSE(ticks.isEmpty());
        for (int i = 1; i < ticks.size(); ++i) {
            EXPECT_NE(axis.formatTick(ticks[i - 1]), axis.formatTick(ticks[i]))
                << "ticks " << ticks[i - 1] << " and " << ticks[i] << " share the label \""
                << axis.formatTick(ticks[i]).toStdString() << "\"";
        }
    }

    TEST(ValueAxisTest, ZeroSpanMinuteAxisNeverRepeatsALabel) {
        // The empty playtime graph: no data, zero baseline, whole-minute labels. Sub-minute ticks
        // used to read "1 min" six times, then "0 min" five times.
        ValueAxis axis({.baseline = ValueAxis::Baseline::Zero}, ValueTransform::secondsToMinutes());
        axis.setRange(0.0, 0.0);
        expectAdjacentLabelsDistinct(axis);
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_GT(axis.max(), 0.0);
    }

    TEST(ValueAxisTest, SubUnitSpanWithWholeUnitLabelsCoarsensTheStep) {
        // Under one minute of playtime: the step grows to a whole minute instead of repeating labels.
        ValueAxis axis({.baseline = ValueAxis::Baseline::Zero}, ValueTransform::secondsToMinutes());
        axis.setRange(0.2, 0.6);
        expectAdjacentLabelsDistinct(axis);
        EXPECT_GE(axis.max(), 0.6);
        for (const qreal tick: axis.ticks()) EXPECT_DOUBLE_EQ(tick, std::round(tick));
    }

    TEST(ValueAxisTest, SubPercentSpanNeverRepeatsAPercentageLabel) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::HugData}, ValueTransform::percentage());
        axis.setRange(63.4, 63.9);
        expectAdjacentLabelsDistinct(axis);
        EXPECT_LE(axis.min(), 63.4);
        EXPECT_GE(axis.max(), 63.9);
    }

    TEST(ValueAxisTest, SinglePointNeverRepeatsALabel) {
        for (const qreal value: {0.0, 0.004, 0.5, 1.0, 42.0, 5300.0, 1e6}) {
            ValueAxis hug({.baseline = ValueAxis::Baseline::HugData});
            hug.setRange(value, value);
            expectAdjacentLabelsDistinct(hug);
            EXPECT_LT(hug.min(), hug.max());

            ValueAxis zero({.baseline = ValueAxis::Baseline::Zero}, ValueTransform::secondsToMinutes());
            zero.setRange(value, value);
            expectAdjacentLabelsDistinct(zero);
        }
    }

    TEST(ValueAxisTest, FractionalSpanWithDefaultFormatterNeverRepeatsALabel) {
        // The default formatter shows at most one decimal, so a 0.02 step would read "0.5" twice.
        ValueAxis axis({.baseline = ValueAxis::Baseline::HugData});
        axis.setRange(0.5, 0.7);
        expectAdjacentLabelsDistinct(axis);
    }

    TEST(ValueAxisTest, RangeSweepNeverRepeatsALabel) {
        const std::vector<std::pair<qreal, qreal>> ranges{
            {0.0, 0.0}, {0.0, 0.01}, {0.1, 0.15}, {0.99, 1.01}, {1.0, 1.0}, {3.0, 4.0},
            {60.0, 105.0}, {63.0, 63.5}, {4900.0, 5700.0}, {0.0, 300.0}, {12.0, 12.25}};
        const std::vector<ValueTransform> transforms{
            ValueTransform::identity(), ValueTransform::percentage(), ValueTransform::secondsToMinutes()};
        for (const auto &[lo, hi]: ranges) {
            for (const auto &transform: transforms) {
                for (const auto baseline: {ValueAxis::Baseline::Zero, ValueAxis::Baseline::HugData}) {
                    for (const bool niceBounds: {true, false}) {
                        ValueAxis axis({.baseline = baseline, .niceBounds = niceBounds}, transform);
                        axis.setRange(lo, hi);
                        SCOPED_TRACE(testing::Message() << "range " << lo << ".." << hi);
                        expectAdjacentLabelsDistinct(axis);
                    }
                }
            }
        }
    }

    // --- Exact bounds: no tick outside the data (run-history "#0"). ---

    TEST(ValueAxisTest, ExactBoundsTickEveryRunForFewRuns) {
        ValueAxis axis({.baseline = ValueAxis::Baseline::HugData, .integral = true, .niceBounds = false});
        axis.setRange(1.0, 8.0);
        EXPECT_DOUBLE_EQ(axis.min(), 1.0);
        EXPECT_DOUBLE_EQ(axis.max(), 8.0);
        EXPECT_EQ(axis.ticks(), (QList<qreal>{1, 2, 3, 4, 5, 6, 7, 8}));
    }

    TEST(ValueAxisTest, ExactBoundsNeverTickOutsideTheData) {
        for (const qreal last: {2.0, 3.0, 11.0, 37.0, 224.0, 360.0, 1001.0}) {
            ValueAxis axis({.baseline = ValueAxis::Baseline::HugData, .integral = true, .niceBounds = false});
            axis.setRange(1.0, last);
            EXPECT_DOUBLE_EQ(axis.min(), 1.0);
            EXPECT_DOUBLE_EQ(axis.max(), last);
            ASSERT_FALSE(axis.ticks().isEmpty());
            for (const qreal tick: axis.ticks()) {
                EXPECT_GE(tick, 1.0) << "range 1.." << last;
                EXPECT_LE(tick, last) << "range 1.." << last;
                EXPECT_DOUBLE_EQ(tick, std::round(tick));
            }
            expectAdjacentLabelsDistinct(axis);
        }
    }

    TEST(ValueAxisTest, ChangingOptionsRecomputesTheCurrentRange) {
        ValueAxis axis;
        axis.setRange(12.0, 87.0);
        axis.setOptions({.baseline = ValueAxis::Baseline::Zero, .integral = true});
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        for (const qreal tick : axis.ticks()) EXPECT_DOUBLE_EQ(tick, std::round(tick));
    }
}
