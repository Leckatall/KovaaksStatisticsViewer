//
// PlaytimeGraphViewModel tests using a hand-written fake IPlaytimeGraphUseCase.
//

#include <gtest/gtest.h>

#include <QDateTime>
#include <QTimeZone>
#include <QSignalSpy>

#include "playtime_graph_vm.h"

using namespace ksv::presentation;
using namespace ksv::application;

namespace {
    qreal epochDayMs(const long long epochDay) {
        return QDateTime(QDate(1970, 1, 1).addDays(epochDay), QTime(0, 0), QTimeZone::utc()).toMSecsSinceEpoch();
    }

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
        const auto series = view_model.series({PlaytimeGraphViewModel::Playtime});
        ASSERT_FALSE(series.isEmpty());
        EXPECT_TRUE(series.front()->points.isEmpty());
    }

    TEST_F(PlaytimeGraphViewModelTest, RefreshConvertsSecondsToMinutesKeepingTheDateTimeAsX) {
        fake->series = {{19500, 1800.0}, {19501, 900.0}};

        view_model.refresh();

        const auto series = view_model.series({PlaytimeGraphViewModel::Playtime});
        ASSERT_FALSE(series.isEmpty());
        const auto points = series.front()->displayPoints();
        ASSERT_EQ(points.size(), 2);
        EXPECT_DOUBLE_EQ(points[0].x(), epochDayMs(19500));
        EXPECT_DOUBLE_EQ(points[0].y(), 30.0); // 1800s / 60
        EXPECT_DOUBLE_EQ(points[1].x(), epochDayMs(19501));
        EXPECT_DOUBLE_EQ(points[1].y(), 15.0); // 900s / 60
    }

    TEST_F(PlaytimeGraphViewModelTest, OnlyPlaytimeColumnIsPlottableAndHasData) {
        fake->series = {{19500, 1800.0}, {19501, 900.0}};
        view_model.refresh();

        EXPECT_FALSE(view_model.series({PlaytimeGraphViewModel::Playtime}).isEmpty());
        // The Date column carries no drawable series of its own.
        EXPECT_TRUE(view_model.series({PlaytimeGraphViewModel::Date}).isEmpty());
    }

    TEST_F(PlaytimeGraphViewModelTest, SinglePointExpandsXAxisToSurroundingCalendarDays) {
        fake->series = {{19500, 1800.0}};
        view_model.refresh();

        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), epochDayMs(19499));
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), epochDayMs(19501));
    }

    TEST_F(PlaytimeGraphViewModelTest, RefreshEmitsDataUpdatedAndBoundsChanged) {
        fake->series = {{19500, 1800.0}};

        const QSignalSpy dataSpy(&view_model, &GraphViewModelBase::dataUpdated);
        const QSignalSpy boundsSpy(&view_model, &GraphViewModelBase::boundsChanged);
        view_model.refresh();

        EXPECT_GT(dataSpy.count(), 0);
        EXPECT_GT(boundsSpy.count(), 0);
    }

    TEST_F(PlaytimeGraphViewModelTest, XAxisDelegateRendersTheEpochMsAsACalendarDate) {
        const qreal dayValue = epochDayMs(19500);
        const QString expected = QDate(1970, 1, 1).addDays(19500).toString("MMM d");
        EXPECT_EQ(view_model.xAxis().formatTick(dayValue), expected);
        EXPECT_FALSE(expected.isEmpty());
    }

    TEST_F(PlaytimeGraphViewModelTest, SeriesSharesTheSameDelegateAsItsYAxis) {
        fake->series = {{19500, 1800.0}};
        view_model.refresh();

        const auto series = view_model.series({PlaytimeGraphViewModel::Playtime});
        ASSERT_EQ(series.size(), 1);
        ASSERT_TRUE(series.front()->yAxis.has_value());
        EXPECT_EQ(series.front()->formattedValueAtX(epochDayMs(19500)), "30 min");
        EXPECT_EQ(series.front()->yAxis->formatTick(30.0), "30 min");
    }

    TEST_F(PlaytimeGraphViewModelTest, SeriesOmitsDateColumnWhichHasNoDrawableSeries) {
        fake->series = {{19500, 1800.0}};
        view_model.refresh();

        EXPECT_TRUE(view_model.series({PlaytimeGraphViewModel::Date}).isEmpty());
    }

    TEST_F(PlaytimeGraphViewModelTest, PlaytimeColumnHasNameAndValidColor) {
        const auto series = view_model.series({PlaytimeGraphViewModel::Playtime});
        ASSERT_EQ(series.size(), 1);
        EXPECT_EQ(series.front()->name(), "Playtime (3-day avg)");
        EXPECT_TRUE(series.front()->color().isValid());
    }
}
