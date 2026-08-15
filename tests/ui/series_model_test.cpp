//
// SeriesModel (display-space projection, setData, and value-anchored
// hit-testing) tests.
//

#include <gtest/gtest.h>

#include <QColor>
#include <QPointF>
#include <array>
#include <cmath>

#include "series_model.h"

using ksv::presentation::AxisModel;
using ksv::presentation::SeriesModel;
using ksv::presentation::ValueTransform;

namespace {
    TEST(SeriesModelTest, DisplayPointsAppliesTransformToYOnly) {
        SeriesModel series;
        series.points = {QPointF(0.0, 0.5), QPointF(1.0, 0.75)};
        series.transform = ValueTransform::percentage();

        const auto display = series.displayPoints();
        ASSERT_EQ(display.size(), 2);
        EXPECT_DOUBLE_EQ(display[0].x(), 0.0);
        EXPECT_DOUBLE_EQ(display[0].y(), 50.0);
        EXPECT_DOUBLE_EQ(display[1].x(), 1.0);
        EXPECT_DOUBLE_EQ(display[1].y(), 75.0);
    }

    TEST(SeriesModelTest, YAxisBuiltOverDisplayRangeYieldsRoundDisplayTicks) {
        // Raw accuracy range [0.12, 0.87] -> display range [12, 87] via
        // percentage(); the axis must land on round PERCENT numbers, not
        // round fractions of the raw ratio.
        const ValueTransform transform = ValueTransform::percentage();
        const AxisModel yAxis = AxisModel::forRange(transform.display(0.12), transform.display(0.87));

        ASSERT_GE(yAxis.ticks().size(), 2);
        const double step = yAxis.ticks()[1] - yAxis.ticks()[0];
        EXPECT_GT(step, 1.0); // a percentage-scale step, not a sub-1 fractional one
        for (const double t: yAxis.ticks()) {
            const double k = t / step;
            EXPECT_NEAR(k, std::round(k), 1e-6);
        }
    }

    TEST(SeriesModelTest, SetDataBuildsYAxisOverDisplayRangeSharingTheDelegate) {
        SeriesModel series;
        series.transform = ValueTransform::percentage();

        series.setData({QPointF(0.0, 0.12), QPointF(1.0, 0.87)});

        ASSERT_TRUE(series.yAxis.has_value());
        EXPECT_LE(series.yAxis->min(), 12.0);
        EXPECT_GE(series.yAxis->max(), 87.0);
        EXPECT_EQ(series.yAxis->formatTick(87.0), "87%");
    }

    TEST(SeriesModelTest, DisplayRangeIsEmptyWithoutPointsAndUsesDisplayValues) {
        SeriesModel empty;
        EXPECT_FALSE(empty.displayRange().has_value());

        SeriesModel series;
        series.transform = ValueTransform::percentage();
        series.points = {QPointF(0.0, 0.12), QPointF(1.0, 0.87)};

        const auto range = series.displayRange();
        ASSERT_TRUE(range.has_value());
        EXPECT_DOUBLE_EQ(range->first, 12.0);
        EXPECT_DOUBLE_EQ(range->second, 87.0);
    }

    TEST(SeriesModelTest, AxisForSeriesUsesTheUnionAndItsEmptyFallback) {
        SeriesModel first;
        first.points = {QPointF(0.0, 10.0), QPointF(1.0, 20.0)};
        SeriesModel second;
        second.points = {QPointF(0.0, 100.0), QPointF(1.0, 200.0)};
        const std::array<const SeriesModel *, 2> members{&first, &second};

        const AxisModel axis = ksv::presentation::axisForSeries(members, {}, ValueTransform::identity());
        EXPECT_LE(axis.min(), 10.0);
        EXPECT_GE(axis.max(), 200.0);

        const AxisModel empty = ksv::presentation::axisForSeries({}, {}, ValueTransform::identity());
        EXPECT_DOUBLE_EQ(empty.min(), 0.0);
        EXPECT_DOUBLE_EQ(empty.max(), 1.0);
    }

    TEST(SeriesModelTest, SampleAtXReturnsNearestSampleByRawX) {
        SeriesModel series;
        series.transform = ValueTransform::percentage();
        series.setData({QPointF(0.0, 0.5), QPointF(1.0, 0.6), QPointF(2.0, 0.87)});

        const auto sample = series.sampleAtX(1.9);
        ASSERT_TRUE(sample.has_value());
        EXPECT_DOUBLE_EQ(sample->x(), 2.0);
        EXPECT_DOUBLE_EQ(sample->y(), 87.0);
    }

    TEST(SeriesModelTest, SampleAtXOnEmptySeriesIsNullopt) {
        SeriesModel series;
        EXPECT_FALSE(series.sampleAtX(5.0).has_value());
    }

    TEST(SeriesModelTest, FormattedValueAtXUsesTheDelegate) {
        SeriesModel series;
        series.transform = ValueTransform::percentage();
        series.setData({QPointF(0.0, 0.5), QPointF(1.0, 0.87)});

        EXPECT_EQ(series.formattedValueAtX(1.0), "87%");
    }

    TEST(SeriesModelTest, FormattedValueAtXOnEmptySeriesIsEmpty) {
        SeriesModel series;
        EXPECT_TRUE(series.formattedValueAtX(5.0).isEmpty());
    }

    TEST(SeriesModelTest, PlaytimeFormattedValueAtXShowsMinutes) {
        SeriesModel series;
        series.transform = ValueTransform::secondsToMinutes();
        series.setData({QPointF(0.0, 1800.0)});

        EXPECT_EQ(series.formattedValueAtX(0.0), "30 min");
    }
}
