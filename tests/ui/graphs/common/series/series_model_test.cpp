//
// SeriesModel (display-space projection, setData, and value-anchored
// hit-testing) tests.
//

#include <gtest/gtest.h>

#include <QColor>
#include <QPointF>
#include <array>
#include <cmath>

#include "components/series_painter.h"
#include "presentation/graphs/common/series/series_model.h"

using ksv::presentation::SeriesModel;
using ksv::presentation::ValueAxis;
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

        const ValueAxis axis = ksv::presentation::axisForSeries(members, {}, ValueTransform::identity());
        EXPECT_LE(axis.min(), 10.0);
        EXPECT_GE(axis.max(), 200.0);

        const ValueAxis empty = ksv::presentation::axisForSeries({}, {}, ValueTransform::identity());
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

    TEST(SeriesModelTest, LineWidthDefaultsToThePainterWidth) {
        EXPECT_DOUBLE_EQ(SeriesModel{}.lineWidth, ksv::ui::SeriesPainter::Style{}.lineWidth);
    }

    TEST(SeriesModelTest, CarriedVerticesAreNeverSampled) {
        SeriesModel series;
        series.points = {QPointF(10.0, 4.0)};
        series.leadingCarry = QPointF(0.0, 1.0);
        series.trailingCarry = QPointF(20.0, 4.0);

        for (const qreal x: {0.0, 5.0, 15.0, 20.0}) {
            const auto sample = series.sampleAtX(x);
            ASSERT_TRUE(sample) << "x=" << x;
            EXPECT_DOUBLE_EQ(sample->x(), 10.0) << "x=" << x;
        }
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
