//
// PlaytimeGraphViewModel tests using a hand-written fake IPlaytimeGraphUseCase.
//

#include <gtest/gtest.h>

#include <QDate>
#include <QSignalSpy>

#include "playtime_graph_vm.h"

using namespace ksv::presentation;
using namespace ksv::application;

namespace {
    class FakePlaytimeUseCase : public IPlaytimeGraphUseCase {
    public:
        std::vector<std::pair<long long, double>> series;
        int last_window_days = 0;

        std::vector<std::pair<long long, double>> get_rolling_playtime(const int window_days) override {
            last_window_days = window_days;
            return series;
        }
    };

    class PlaytimeGraphViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakePlaytimeUseCase> fake = std::make_shared<FakePlaytimeUseCase>();
        PlaytimeGraphViewModel view_model{fake};
    };

    TEST_F(PlaytimeGraphViewModelTest, RequestsThreeDayWindow) {
        EXPECT_EQ(fake->last_window_days, 3);
    }

    TEST_F(PlaytimeGraphViewModelTest, StartsEmpty) {
        EXPECT_TRUE(view_model.seriesPoints(PlaytimeGraphViewModel::Playtime).isEmpty());
    }

    TEST_F(PlaytimeGraphViewModelTest, RefreshConvertsSecondsToMinutesKeepingTheDayAsX) {
        fake->series = {{19500, 1800.0}, {19501, 900.0}};

        view_model.refresh();

        const auto points = view_model.seriesPoints(PlaytimeGraphViewModel::Playtime);
        ASSERT_EQ(points.size(), 2);
        EXPECT_DOUBLE_EQ(points[0].x(), 19500.0);
        EXPECT_DOUBLE_EQ(points[0].y(), 30.0); // 1800s / 60
        EXPECT_DOUBLE_EQ(points[1].x(), 19501.0);
        EXPECT_DOUBLE_EQ(points[1].y(), 15.0); // 900s / 60
    }

    TEST_F(PlaytimeGraphViewModelTest, OnlyPlaytimeColumnIsPlottableAndHasData) {
        fake->series = {{19500, 1800.0}, {19501, 900.0}};
        view_model.refresh();

        const auto plottable = view_model.plottableColumns();
        ASSERT_EQ(plottable.size(), 1);
        EXPECT_EQ(plottable[0].toInt(), int(PlaytimeGraphViewModel::Playtime));

        // The Date column carries no drawable series of its own.
        EXPECT_TRUE(view_model.seriesPoints(PlaytimeGraphViewModel::Date).isEmpty());
    }

    TEST_F(PlaytimeGraphViewModelTest, AxisBoundsSpanDaysAndStartYAtZeroOnNiceNumbers) {
        fake->series = {{19500, 1800.0}, {19502, 3600.0}}; // 30 min, 60 min
        view_model.refresh();

        const auto bounds = view_model.axisBounds();
        const auto xBounds = bounds[QString::number(PlaytimeGraphViewModel::Date)].toPointF();
        const auto yBounds = bounds[QString::number(PlaytimeGraphViewModel::Playtime)].toPointF();

        // X spans the days on a whole-day (integral) grid.
        EXPECT_DOUBLE_EQ(xBounds.x(), 19500.0);
        EXPECT_DOUBLE_EQ(xBounds.y(), 19502.0);
        EXPECT_EQ(view_model.axisTicks(PlaytimeGraphViewModel::Date),
                  (QList<qreal>{19500.0, 19501.0, 19502.0}));

        // Y is zero-based and rounds up to a nice value with round ticks.
        EXPECT_DOUBLE_EQ(yBounds.x(), 0.0);
        EXPECT_DOUBLE_EQ(yBounds.y(), 60.0);
        const auto yTicks = view_model.axisTicks(PlaytimeGraphViewModel::Playtime);
        ASSERT_GE(yTicks.size(), 2);
        EXPECT_DOUBLE_EQ(yTicks.front(), 0.0);
        EXPECT_DOUBLE_EQ(yTicks.back(), 60.0);
    }

    TEST_F(PlaytimeGraphViewModelTest, SinglePointExpandsXAxisToNiceWholeDays) {
        fake->series = {{19500, 1800.0}};
        view_model.refresh();

        const auto xBounds = view_model.axisBounds()[QString::number(PlaytimeGraphViewModel::Date)].toPointF();
        EXPECT_DOUBLE_EQ(xBounds.x(), 19499.0);
        EXPECT_DOUBLE_EQ(xBounds.y(), 19501.0);
    }

    TEST_F(PlaytimeGraphViewModelTest, RefreshEmitsDataUpdatedAndBoundsChanged) {
        fake->series = {{19500, 1800.0}};

        const QSignalSpy dataSpy(&view_model, &GraphViewModelBase::dataUpdated);
        const QSignalSpy boundsSpy(&view_model, &GraphViewModelBase::boundsChanged);
        view_model.refresh();

        EXPECT_GT(dataSpy.count(), 0);
        EXPECT_GT(boundsSpy.count(), 0);
    }

    TEST_F(PlaytimeGraphViewModelTest, XAxisDelegateRendersTheEpochDayAsACalendarDate) {
        // X values are days since 1970-01-01; the axis must show real dates.
        const qreal dayValue = 19500.0;
        const QString expected = QDate(1970, 1, 1).addDays(19500).toString("MMM d");
        EXPECT_EQ(view_model.xAxis().formatTick(dayValue), expected);
        EXPECT_FALSE(expected.isEmpty());
    }

    TEST_F(PlaytimeGraphViewModelTest, SeriesSharesTheSameDelegateAsItsYAxis) {
        fake->series = {{19500, 1800.0}};
        view_model.refresh();

        const auto series = view_model.series({PlaytimeGraphViewModel::Playtime});
        ASSERT_EQ(series.size(), 1);
        ASSERT_TRUE(series.front().yAxis.has_value());
        EXPECT_EQ(series.front().formattedValueAtX(19500.0), "30 min");
        EXPECT_EQ(series.front().yAxis->formatTick(30.0), "30 min");
    }

    TEST_F(PlaytimeGraphViewModelTest, SeriesOmitsDateColumnWhichHasNoDrawableSeries) {
        fake->series = {{19500, 1800.0}};
        view_model.refresh();

        EXPECT_TRUE(view_model.series({PlaytimeGraphViewModel::Date}).isEmpty());
    }

    TEST_F(PlaytimeGraphViewModelTest, PlaytimeColumnHasNameAndValidColor) {
        EXPECT_EQ(view_model.columnName(PlaytimeGraphViewModel::Playtime), "Playtime (3-day avg)");
        EXPECT_TRUE(view_model.columnColor(PlaytimeGraphViewModel::Playtime).isValid());
    }
}
