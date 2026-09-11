//
// ValueAxis (nice-number bounds + ticks) tests.
//

#include <gtest/gtest.h>

#include <cmath>

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

    TEST(ValueAxisTest, DefaultDelegateFormatsWithDefaultTickRules) {
        ValueAxis axis;
        axis.setRange(0.0, 10.0);
        EXPECT_EQ(axis.formatTick(5.0), "5");
        EXPECT_EQ(axis.formatTick(5.25), "5.3");
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

    TEST(ValueAxisTest, ChangingOptionsRecomputesTheCurrentRange) {
        ValueAxis axis;
        axis.setRange(12.0, 87.0);
        axis.setOptions({.baseline = ValueAxis::Baseline::Zero, .integral = true});
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        for (const qreal tick : axis.ticks()) EXPECT_DOUBLE_EQ(tick, std::round(tick));
    }
}
