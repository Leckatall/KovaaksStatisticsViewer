//
// Numeric Y-axis policies: fit visible data, start at zero and exact fixed bounds, in displayed units.
//

#include <gtest/gtest.h>

#include <QList>
#include <QPointF>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "presentation/axis_policy.h"

using namespace ksv::presentation;
using namespace ksv::application;

namespace {
    SeriesModel &seriesOf(SeriesModel &series, QList<QPointF> points, const ValueTransform &transform = {}) {
        series.transform = transform;
        series.points = std::move(points);
        return series;
    }

    ValueAxis axisOver(const std::vector<const SeriesModel *> &members, const AxisPolicy &policy,
                       const ValueAxis::Options &base = {}, const ValueTransform &transform = {}) {
        return axisForPolicy(members, policy, base, transform);
    }

    void expectSameAxis(const Axis &actual, const Axis &expected) {
        EXPECT_DOUBLE_EQ(actual.min(), expected.min());
        EXPECT_DOUBLE_EQ(actual.max(), expected.max());
        EXPECT_EQ(actual.ticks(), expected.ticks());
    }

    TEST(AxisPolicyTest, FitVisibleDataSpansOnlyTheMembersItIsGiven) {
        SeriesModel hits;
        SeriesModel shots;
        seriesOf(hits, {QPointF(1, 9), QPointF(2, 15)});
        seriesOf(shots, {QPointF(1, 20), QPointF(2, 25)});

        const ValueAxis hitsOnly = axisOver({&hits}, FitVisibleData{});
        const ValueAxis both = axisOver({&hits, &shots}, FitVisibleData{});

        ValueAxis expectedHitsOnly;
        expectedHitsOnly.setRange(9.0, 15.0);
        ValueAxis expectedBoth;
        expectedBoth.setRange(9.0, 25.0);
        expectSameAxis(hitsOnly, expectedHitsOnly);
        expectSameAxis(both, expectedBoth);
        EXPECT_GT(hitsOnly.min(), 0.0) << "fit hugs the data, it does not reach for zero";
    }

    TEST(AxisPolicyTest, StartAtZeroPinsTheLowerBoundWhateverTheDataMinimum) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 40), QPointF(2, 90)});

        const ValueAxis axis = axisOver({&series}, StartAtZero{});

        ValueAxis expected({.baseline = ValueAxis::Baseline::Zero});
        expected.setRange(40.0, 90.0);
        expectSameAxis(axis, expected);
        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_GE(axis.max(), 90.0);
    }

    TEST(AxisPolicyTest, FitAndZeroPoliciesOverrideTheBaselineOfTheBaseOptions) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 40), QPointF(2, 90)});

        const ValueAxis fitted = axisOver({&series}, FitVisibleData{}, {.baseline = ValueAxis::Baseline::Zero});
        const ValueAxis zeroed = axisOver({&series}, StartAtZero{}, {.baseline = ValueAxis::Baseline::HugData});

        EXPECT_GT(fitted.min(), 0.0);
        EXPECT_DOUBLE_EQ(zeroed.min(), 0.0);
    }

    TEST(AxisPolicyTest, FixedBoundsAreExactAndNeverWidenedToNiceNumbers) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 3), QPointF(2, 97)});

        const ValueAxis axis = axisOver({&series}, FixedBounds{12.5, 47.3});

        EXPECT_DOUBLE_EQ(axis.min(), 12.5);
        EXPECT_DOUBLE_EQ(axis.max(), 47.3);
        ASSERT_FALSE(axis.ticks().isEmpty());
        for (const qreal tick: axis.ticks()) {
            EXPECT_GE(tick, 12.5);
            EXPECT_LE(tick, 47.3);
        }
    }

    TEST(AxisPolicyTest, FixedBoundsKeepReadableInteriorTicks) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 3)});

        const ValueAxis axis = axisOver({&series}, FixedBounds{12.5, 47.3});

        EXPECT_GE(axis.ticks().size(), 3);
        const qreal step = axis.ticks()[1] - axis.ticks()[0];
        for (const qreal tick: axis.ticks()) EXPECT_NEAR(tick / step, std::round(tick / step), 1e-9) << "ticks are nice multiples";
    }

    TEST(AxisPolicyTest, FixedAccuracyBoundsArePercentagesOfTheFractionPlotted) {
        SeriesModel accuracy;
        seriesOf(accuracy, {QPointF(1, 0.20), QPointF(2, 0.90)}, ValueTransform::percentage());

        const ValueAxis axis = axisOver({&accuracy}, FixedBounds{25.0, 75.0}, {}, ValueTransform::percentage());

        EXPECT_DOUBLE_EQ(axis.min(), 25.0);
        EXPECT_DOUBLE_EQ(axis.max(), 75.0);
        EXPECT_EQ(axis.formatTick(axis.max()), "75%");
        EXPECT_DOUBLE_EQ(axis.normalizedPosition(accuracy.transform.display(0.50)), 0.5)
            << "the fraction 0.5 is the displayed 50%, midway through 25..75";
    }

    TEST(AxisPolicyTest, FixedPlaytimeBoundsAreMinutesOfTheSecondsPlotted) {
        SeriesModel playtime;
        seriesOf(playtime, {QPointF(1, 600.0), QPointF(2, 7200.0)}, ValueTransform::secondsToMinutes());

        const ValueAxis axis = axisOver({&playtime}, FixedBounds{0.0, 30.0}, {}, ValueTransform::secondsToMinutes());

        EXPECT_DOUBLE_EQ(axis.min(), 0.0);
        EXPECT_DOUBLE_EQ(axis.max(), 30.0);
        EXPECT_EQ(axis.formatTick(30.0), "30 min");
        EXPECT_DOUBLE_EQ(axis.normalizedPosition(playtime.transform.display(900.0)), 0.5) << "15 min of 0..30";
    }

    TEST(AxisPolicyTest, FixedBoundsHoldWithNoMembersAtAll) {
        const ValueAxis axis = axisOver({}, FixedBounds{-5.0, 5.0});

        EXPECT_DOUBLE_EQ(axis.min(), -5.0);
        EXPECT_DOUBLE_EQ(axis.max(), 5.0);
    }

    TEST(AxisPolicyTest, FitOfASingletonUsesTheExistingFallbackSpan) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 50.0)});

        const ValueAxis axis = axisOver({&series}, FitVisibleData{});

        ValueAxis expected;
        expected.setRange(50.0, 50.0);
        expectSameAxis(axis, expected);
        EXPECT_LT(axis.min(), 50.0);
        EXPECT_GT(axis.max(), 50.0);
    }

    TEST(AxisPolicyTest, FitWithoutAnyMemberFallsBackToTheEmptyRange) {
        ValueAxis expected;
        expected.setRange(0.0, 1.0);

        expectSameAxis(axisOver({}, FitVisibleData{}), expected);
        const SeriesModel unfilled;
        expectSameAxis(axisOver({&unfilled}, FitVisibleData{}), expected);
    }

    TEST(AxisPolicyTest, StartAtZeroOfASingletonUsesTheExistingFallbackSpan) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 0.0)});

        const ValueAxis axis = axisOver({&series}, StartAtZero{});

        ValueAxis expected({.baseline = ValueAxis::Baseline::Zero});
        expected.setRange(0.0, 0.0);
        expectSameAxis(axis, expected);
        EXPECT_GT(axis.max(), axis.min());
    }

    TEST(AxisPolicyTest, CarriedStateCountsAsPlottedData) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 10.0)});
        series.leadingCarry = QPointF(0, 40.0);

        const ValueAxis axis = axisOver({&series}, FitVisibleData{});

        ValueAxis expected;
        expected.setRange(10.0, 40.0);
        expectSameAxis(axis, expected);
    }

    TEST(AxisPolicyTest, UnusableFixedBoundsFallBackToFittingTheData) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 3), QPointF(2, 9)});
        const qreal nan = std::numeric_limits<qreal>::quiet_NaN();
        ValueAxis fitted;
        fitted.setRange(3.0, 9.0);

        expectSameAxis(axisOver({&series}, FixedBounds{5.0, 5.0}), fitted);
        expectSameAxis(axisOver({&series}, FixedBounds{9.0, 3.0}), fitted);
        expectSameAxis(axisOver({&series}, FixedBounds{nan, 3.0}), fitted);
    }

    TEST(AxisPolicyTest, OtherBaseOptionsSurviveThePolicy) {
        SeriesModel series;
        seriesOf(series, {QPointF(1, 0.0), QPointF(2, 2.0)});

        const ValueAxis axis = axisOver({&series}, FixedBounds{0.0, 2.0}, {.integral = true});

        ASSERT_FALSE(axis.ticks().isEmpty());
        for (const qreal tick: axis.ticks()) EXPECT_DOUBLE_EQ(tick, std::round(tick));
    }

    TEST(AxisPolicyTest, PolicyLookupTakesTheStoredPolicyOrTheGraphsDefault) {
        const AxisPolicyMap stored{{"score", FixedBounds{10.0, 20.0}}};

        EXPECT_EQ(axisPolicyFor(stored, GraphKey::ScenarioHistory, "score"), (AxisPolicy{FixedBounds{10.0, 20.0}}));
        const auto defaultOf = [](const GraphKey graph, const std::string &axis) {
            return defaultGraphPreferences(graph).axisPolicies.at(axis);
        };
        EXPECT_EQ(axisPolicyFor(stored, GraphKey::ScenarioHistory, "accuracy"),
                  defaultOf(GraphKey::ScenarioHistory, "accuracy"));
        EXPECT_EQ(axisPolicyFor(stored, GraphKey::ScenarioHistory, "counts"),
                  defaultOf(GraphKey::ScenarioHistory, "counts"));
        EXPECT_EQ(axisPolicyFor({}, GraphKey::DailyPlaytime, "playtime"), defaultOf(GraphKey::DailyPlaytime, "playtime"));
        EXPECT_EQ(axisPolicyFor({}, GraphKey::BenchmarkPlaytime, "playtime"),
                  defaultOf(GraphKey::BenchmarkPlaytime, "playtime"));
    }

    TEST(AxisPolicyTest, AGraphWithoutThatAxisHasNothingToLookUp) {
        EXPECT_EQ(axisPolicyFor({}, GraphKey::BenchmarkRank, "rank"), AxisPolicy{FitVisibleData{}});
        EXPECT_EQ(axisPolicyFor({}, GraphKey::Run, "score"), AxisPolicy{FitVisibleData{}});
    }
}
