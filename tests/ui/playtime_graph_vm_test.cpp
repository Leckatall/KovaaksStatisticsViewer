//
// PlaytimeGraphViewModel tests using a hand-written fake IPlaytimeGraphUseCase.
//

#include <gtest/gtest.h>

#include <QDateTime>
#include <QTimeZone>
#include <QSignalSpy>

#include <functional>

#include "playtime_graph_vm.h"

using namespace ksv::presentation;
using namespace ksv::application;

namespace {
    qreal epochDayMs(const long long epochDay) {
        return QDateTime(QDate(1970, 1, 1).addDays(epochDay), QTime(0, 0), QTimeZone::utc()).toMSecsSinceEpoch();
    }

    // Samples are given as epoch days, the unit the assertions below use.
    class FakePlaytimeUseCase : public IPlaytimeGraphUseCase {
    public:
        std::vector<std::pair<long long, double>> series;
        PlaytimeView view;
        std::vector<std::function<void()>> callbacks;

        [[nodiscard]] PlaytimeGraphSnapshot snapshot() override {
            PlaytimeGraphSnapshot result;
            result.view = view;
            for (const auto &[epochDay, seconds]: series)
                result.samples.emplace_back(std::chrono::sys_days{std::chrono::days{epochDay}}, seconds);
            return result;
        }

        void onChanged(std::function<void()> callback) override { callbacks.push_back(std::move(callback)); }

        void publish() const {
            for (const auto &callback: callbacks) callback();
        }
    };

    class PlaytimeGraphViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakePlaytimeUseCase> fake = std::make_shared<FakePlaytimeUseCase>();
        PlaytimeGraphViewModel view_model{fake};
    };

    TEST_F(PlaytimeGraphViewModelTest, DefaultWindowIsThreeDays) {
        EXPECT_EQ(view_model.averagingWindowDays(), 3);
    }

    TEST_F(PlaytimeGraphViewModelTest, UseCaseChangesRefreshTheGraph) {
        fake->series = {{19500, 1800.0}};
        const QSignalSpy dataSpy(&view_model, &GraphViewModelBase::dataUpdated);

        fake->publish();

        EXPECT_GT(dataSpy.count(), 0);
        EXPECT_TRUE(view_model.hasData());
    }

    TEST_F(PlaytimeGraphViewModelTest, TitleAndSeriesNameStateTheDefaultWindow) {
        EXPECT_EQ(view_model.title(), "Daily Playtime (3-day rolling average)");
        EXPECT_EQ(view_model.series({PlaytimeGraphViewModel::Playtime}).front()->name(), "Playtime (3-day avg)");
    }

    TEST_F(PlaytimeGraphViewModelTest, TitleAndSeriesNameFollowAnyOtherWindowAndNeverSayThree) {
        fake->view.windowDays = 14;
        const QSignalSpy windowSpy(&view_model, &PlaytimeGraphViewModel::windowChanged);

        fake->publish();

        EXPECT_EQ(windowSpy.count(), 1);
        EXPECT_EQ(view_model.averagingWindowDays(), 14);
        EXPECT_EQ(view_model.title(), "Daily Playtime (14-day rolling average)");
        EXPECT_EQ(view_model.series({PlaytimeGraphViewModel::Playtime}).front()->name(), "Playtime (14-day avg)");
        EXPECT_FALSE(view_model.title().contains("3-day"));
    }

    TEST_F(PlaytimeGraphViewModelTest, OneDayWindowReadsAsDailyTotals) {
        fake->view.windowDays = 1;

        fake->publish();

        EXPECT_EQ(view_model.title(), "Daily Playtime (daily totals)");
        EXPECT_EQ(view_model.series({PlaytimeGraphViewModel::Playtime}).front()->name(), "Playtime (daily total)");
    }

    TEST_F(PlaytimeGraphViewModelTest, UnchangedWindowDoesNotAnnounceAWindowChange) {
        const QSignalSpy windowSpy(&view_model, &PlaytimeGraphViewModel::windowChanged);
        fake->series = {{19500, 1800.0}};

        fake->publish();

        EXPECT_EQ(windowSpy.count(), 0);
    }

    TEST_F(PlaytimeGraphViewModelTest, ADateRangeSpansItsWholeIntervalIncludingBlankDays) {
        fake->series = {{19503, 600.0}};
        fake->view.interval = DayInterval{std::chrono::sys_days{std::chrono::days{19500}},
                                          std::chrono::sys_days{std::chrono::days{19506}}};

        fake->publish();

        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), epochDayMs(19500));
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), epochDayMs(19506));
        const auto series = view_model.series({PlaytimeGraphViewModel::Playtime});
        ASSERT_EQ(series.front()->points.size(), 1);
    }

    TEST_F(PlaytimeGraphViewModelTest, AnEmptyIntervalKeepsItsAxisButHasNoData) {
        fake->view.interval = DayInterval{std::chrono::sys_days{std::chrono::days{19500}},
                                          std::chrono::sys_days{std::chrono::days{19506}}};

        fake->publish();

        EXPECT_FALSE(view_model.hasData());
        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), epochDayMs(19500));
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), epochDayMs(19506));
    }

    TEST_F(PlaytimeGraphViewModelTest, WithoutAnIntervalTheAxisIsTheSamplesExtentAsBefore) {
        fake->series = {{19500, 600.0}, {19504, 600.0}};

        fake->publish();

        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), epochDayMs(19500));
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), epochDayMs(19504));
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

    TEST_F(PlaytimeGraphViewModelTest, XAxisIdentityIsStableAcrossRefresh) {
        fake->series = {{19500, 1800.0}};
        const Axis *const before = &view_model.xAxis();

        view_model.refresh();

        EXPECT_EQ(&view_model.xAxis(), before);
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
        // A 10-day span selects day-granularity ticks (see DateTimeAxis's calendar-interval
        // selection), so the label carries no time component.
        fake->series = {{19500, 1800.0}, {19510, 900.0}};
        view_model.refresh();

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

    TEST_F(PlaytimeGraphViewModelTest, PlaytimeColumnHasNameAndValidColor) {
        const auto series = view_model.series({PlaytimeGraphViewModel::Playtime});
        ASSERT_EQ(series.size(), 1);
        EXPECT_FALSE(series.front()->name().isEmpty());
        EXPECT_TRUE(series.front()->color().isValid());
    }

    TEST_F(PlaytimeGraphViewModelTest, HasNoDataUntilPlaytimeIsRecorded) {
        EXPECT_FALSE(view_model.hasData());

        fake->series = {{19500, 1800.0}};
        const QSignalSpy dataSpy(&view_model, &GraphViewModelBase::dataUpdated);
        view_model.refresh();
        EXPECT_TRUE(view_model.hasData());
        EXPECT_GT(dataSpy.count(), 0);

        fake->series.clear();
        view_model.refresh();
        EXPECT_FALSE(view_model.hasData());
    }

    TEST_F(PlaytimeGraphViewModelTest, DailyDataIsTickedByDayNotByHour) {
        // Four days of data once ticked every 6 hours ("Aug 25 06:00"), labels the graph cannot fit.
        fake->series = {{19500, 60.0}, {19501, 60.0}, {19502, 60.0}, {19503, 60.0}};
        view_model.refresh();

        for (const qreal tick: view_model.xAxis().ticks()) {
            EXPECT_EQ(QDateTime::fromMSecsSinceEpoch(qint64(tick), QTimeZone::utc()).time(), QTime(0, 0));
        }
    }

    TEST_F(PlaytimeGraphViewModelTest, ConstantSubMinutePlaytimeNeverRepeatsAYAxisLabel) {
        fake->series = {{19500, 30.0}, {19501, 30.0}};
        view_model.refresh();

        const auto series = view_model.series({PlaytimeGraphViewModel::Playtime});
        ASSERT_EQ(series.size(), 1);
        ASSERT_TRUE(series.front()->yAxis.has_value());
        const auto &axis = *series.front()->yAxis;
        for (int i = 1; i < axis.ticks().size(); ++i)
            EXPECT_NE(axis.formatTick(axis.ticks()[i - 1]), axis.formatTick(axis.ticks()[i]));
    }
}
