//
// AxisModel (nice-number bounds + ticks) tests.
//

#include <gtest/gtest.h>

#include <cmath>

#include "axis_model.h"

using ksv::presentation::AxisModel;
using ksv::presentation::ValueTransform;

namespace {
    // Every tick is an (integer) multiple of the spacing between the first two.
    void expectTicksOnRoundGrid(const AxisModel &axis) {
        const auto &ticks = axis.ticks();
        ASSERT_GE(ticks.size(), 2);
        const double step = ticks[1] - ticks[0];
        ASSERT_GT(step, 0.0);
        for (const double t: ticks) {
            const double k = t / step;
            EXPECT_NEAR(k, std::round(k), 1e-6) << "tick " << t << " is not a multiple of step " << step;
        }
    }

    TEST(AxisModelTest, TicksLandOnRoundMultiples) {
        const AxisModel axis = AxisModel::forRange(10.0, 30.0);
        EXPECT_DOUBLE_EQ(axis.min(), 10.0);
        EXPECT_DOUBLE_EQ(axis.max(), 30.0);
        EXPECT_DOUBLE_EQ(axis.ticks().front(), 10.0);
        EXPECT_DOUBLE_EQ(axis.ticks().back(), 30.0);
        expectTicksOnRoundGrid(axis);
    }

    TEST(AxisModelTest, ZeroBaselinePinsMinToZero) {
        const AxisModel axis = AxisModel::forRange(12.0, 87.0, {AxisModel::Baseline::Zero});
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_GE(axis.max(), 87.0);
        EXPECT_DOUBLE_EQ(axis.ticks().front(), 0.0);
        expectTicksOnRoundGrid(axis);
    }

    TEST(AxisModelTest, HugDataRoundsMinDownToANiceValue) {
        // Data starts well above zero; HugData keeps detail by rounding the
        // lower bound DOWN to a round value rather than pinning it to zero.
        const AxisModel axis = AxisModel::forRange(910.0, 985.0, {AxisModel::Baseline::HugData});
        EXPECT_GT(axis.min(), 0.0);
        EXPECT_LE(axis.min(), 910.0);
        EXPECT_GE(axis.max(), 985.0);
        expectTicksOnRoundGrid(axis);
    }

    TEST(AxisModelTest, FractionalRangeStillProducesRoundTicks) {
        const AxisModel axis = AxisModel::forRange(0.5, 0.7, {AxisModel::Baseline::HugData});
        EXPECT_DOUBLE_EQ(axis.min(), 0.5);
        EXPECT_DOUBLE_EQ(axis.max(), 0.7);
        EXPECT_NEAR(axis.ticks().front(), 0.5, 1e-9);
        EXPECT_NEAR(axis.ticks().back(), 0.7, 1e-9);
        expectTicksOnRoundGrid(axis);
    }

    TEST(AxisModelTest, IntegralModeNeverProducesFractionalTicks) {
        // A range that would otherwise pick a sub-1 step (e.g. 0.5) must be
        // forced to a whole-number step so day/second labels never duplicate.
        const AxisModel axis = AxisModel::forRange(0.0, 2.0, {AxisModel::Baseline::Zero, /*integral=*/true});
        for (const double t: axis.ticks()) {
            EXPECT_DOUBLE_EQ(t, std::round(t)) << "integral axis produced fractional tick " << t;
        }
        const double step = axis.ticks()[1] - axis.ticks()[0];
        EXPECT_GE(step, 1.0);
    }

    TEST(AxisModelTest, DegenerateRangeExpandsToNonZeroWidth) {
        const AxisModel axis = AxisModel::forRange(42.0, 42.0, {AxisModel::Baseline::HugData});
        EXPECT_LT(axis.min(), axis.max());
        EXPECT_LE(axis.min(), 42.0);
        EXPECT_GE(axis.max(), 42.0);
        expectTicksOnRoundGrid(axis);
    }

    TEST(AxisModelTest, ZeroBaselineDegenerateRangeStaysGroundedAtZero) {
        const AxisModel axis = AxisModel::forRange(0.0, 0.0, {AxisModel::Baseline::Zero});
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_GT(axis.max(), 0.0);
    }

    TEST(AxisModelTest, TickCountIsCloseToTarget) {
        const AxisModel axis =
                AxisModel::forRange(0.0, 100.0, {AxisModel::Baseline::Zero, /*integral=*/false, /*targetTicks=*/5});
        // Nice-number rounding means the realized count varies a little.
        EXPECT_GE(axis.ticks().size(), 4);
        EXPECT_LE(axis.ticks().size(), 8);
    }

    TEST(AxisModelTest, AllTicksStayWithinBounds) {
        const AxisModel axis = AxisModel::forRange(3.0, 97.0, {AxisModel::Baseline::HugData});
        for (const double t: axis.ticks()) {
            EXPECT_GE(t, axis.min() - 1e-9);
            EXPECT_LE(t, axis.max() + 1e-9);
        }
        EXPECT_DOUBLE_EQ(axis.ticks().front(), axis.min());
        EXPECT_DOUBLE_EQ(axis.ticks().back(), axis.max());
    }

    TEST(AxisModelTest, DefaultConstructedAxisIsUnitRange) {
        const AxisModel axis;
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_DOUBLE_EQ(axis.max(), 1.0);
        EXPECT_FALSE(axis.ticks().isEmpty());
    }

    TEST(AxisModelTest, NormalizedPositionAndValueAtRoundTrip) {
        const AxisModel axis = AxisModel::forRange(10.0, 30.0);
        for (const double value: {10.0, 15.0, 22.5, 30.0}) {
            const double t = axis.normalizedPosition(value);
            EXPECT_NEAR(axis.valueAt(t), value, 1e-9);
        }
        EXPECT_DOUBLE_EQ(axis.normalizedPosition(10.0), 0.0);
        EXPECT_DOUBLE_EQ(axis.normalizedPosition(30.0), 1.0);
        EXPECT_DOUBLE_EQ(axis.valueAt(0.0), 10.0);
        EXPECT_DOUBLE_EQ(axis.valueAt(1.0), 30.0);
    }

    TEST(AxisModelTest, FormatTickUsesTheAttachedDelegate) {
        const AxisModel axis = AxisModel::forRange(0.0, 100.0).withDelegate(ValueTransform::percentage());
        EXPECT_EQ(axis.formatTick(40.0), "40%");
    }

    TEST(AxisModelTest, DefaultDelegateFormatsWithDefaultTickRules) {
        const AxisModel axis = AxisModel::forRange(0.0, 10.0);
        EXPECT_EQ(axis.formatTick(5.0), "5");
        EXPECT_EQ(axis.formatTick(5.25), "5.3");
    }
}
