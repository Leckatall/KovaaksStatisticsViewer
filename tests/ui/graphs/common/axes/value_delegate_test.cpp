//
// ValueDelegate (display conversion and labels) tests.
//

#include <gtest/gtest.h>

#include "presentation/graphs/common/axes/value_delegate.h"

using ksv::presentation::ValueDelegate;

namespace {
    TEST(ValueDelegateTest, IdentityLeavesValueUnchanged) {
        const auto t = ValueDelegate::identity();
        EXPECT_DOUBLE_EQ(t.display(0.42), 0.42);
        EXPECT_DOUBLE_EQ(t.display(-3.0), -3.0);
    }

    TEST(ValueDelegateTest, IdentityFormatsWithDefaultTickRules) {
        const auto t = ValueDelegate::identity();
        EXPECT_EQ(t.axisLabel(5.0), "5");
        EXPECT_EQ(t.axisLabel(5.25), "5.3");
    }

    TEST(ValueDelegateTest, PercentageScalesByOneHundred) {
        const auto t = ValueDelegate::percentage();
        EXPECT_DOUBLE_EQ(t.display(0.0), 0.0);
        EXPECT_DOUBLE_EQ(t.display(0.4), 40.0);
        EXPECT_DOUBLE_EQ(t.display(1.0), 100.0);
    }

    TEST(ValueDelegateTest, PercentageFormatsWithPercentSign) {
        const auto t = ValueDelegate::percentage();
        EXPECT_EQ(t.axisLabel(40.0), "40%");
        EXPECT_EQ(t.axisLabel(t.display(0.873)), "87%");
    }

    TEST(ValueDelegateTest, SecondsToMinutesDividesBySixty) {
        const auto t = ValueDelegate::secondsToMinutes();
        EXPECT_DOUBLE_EQ(t.display(1800.0), 30.0);
        EXPECT_DOUBLE_EQ(t.display(90.0), 1.5);
    }

    TEST(ValueDelegateTest, SecondsToMinutesFormatsWithUnitSuffix) {
        const auto t = ValueDelegate::secondsToMinutes();
        EXPECT_EQ(t.axisLabel(t.display(1800.0)), "30 min");
    }

    TEST(ValueDelegateTest, HoverLabelFallsBackToTheAxisLabel) {
        ValueDelegate delegate;
        delegate.axisFormatter = [](const qreal value) { return QString::number(value) + " pts"; };
        EXPECT_EQ(delegate.hoverLabel(12.0), delegate.axisLabel(12.0));

        const auto percentage = ValueDelegate::percentage();
        EXPECT_EQ(percentage.hoverLabel(87.0), percentage.axisLabel(87.0));
    }

    TEST(ValueDelegateTest, HoverLabelUsesItsOwnFormatterWhenSet) {
        ValueDelegate delegate;
        delegate.axisFormatter = [](const qreal value) { return "axis " + QString::number(value); };
        delegate.hoverFormatter = [](const qreal value) { return "hover " + QString::number(value); };
        EXPECT_EQ(delegate.axisLabel(3.0), "axis 3");
        EXPECT_EQ(delegate.hoverLabel(3.0), "hover 3");
    }
}
