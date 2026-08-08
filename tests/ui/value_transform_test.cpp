//
// ValueTransform (plot-value -> display-value mapping) tests.
//

#include <gtest/gtest.h>

#include "value_transform.h"

using ksv::presentation::ValueTransform;

namespace {
    TEST(ValueTransformTest, IdentityLeavesValueUnchanged) {
        const auto t = ValueTransform::identity();
        EXPECT_DOUBLE_EQ(t.display(0.42), 0.42);
        EXPECT_DOUBLE_EQ(t.display(-3.0), -3.0);
    }

    TEST(ValueTransformTest, IdentityFormatsWithDefaultTickRules) {
        const auto t = ValueTransform::identity();
        EXPECT_EQ(t.format(5.0), "5");
        EXPECT_EQ(t.format(5.25), "5.3");
    }

    TEST(ValueTransformTest, PercentageScalesByOneHundred) {
        const auto t = ValueTransform::percentage();
        EXPECT_DOUBLE_EQ(t.display(0.0), 0.0);
        EXPECT_DOUBLE_EQ(t.display(0.4), 40.0);
        EXPECT_DOUBLE_EQ(t.display(1.0), 100.0);
    }

    TEST(ValueTransformTest, PercentageFormatsWithPercentSign) {
        const auto t = ValueTransform::percentage();
        EXPECT_EQ(t.format(40.0), "40%");
        EXPECT_EQ(t.format(t.display(0.873)), "87%");
    }

    TEST(ValueTransformTest, SecondsToMinutesDividesBySixty) {
        const auto t = ValueTransform::secondsToMinutes();
        EXPECT_DOUBLE_EQ(t.display(1800.0), 30.0);
        EXPECT_DOUBLE_EQ(t.display(90.0), 1.5);
    }

    TEST(ValueTransformTest, SecondsToMinutesFormatsWithUnitSuffix) {
        const auto t = ValueTransform::secondsToMinutes();
        EXPECT_EQ(t.format(t.display(1800.0)), "30 min");
    }
}
