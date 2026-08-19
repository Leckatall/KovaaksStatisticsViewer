#include <gtest/gtest.h>

#include <QSet>
#include <QSignalSpy>

#include <cmath>
#include <memory>
#include <utility>

#include "completion_history_vm.h"

using namespace ksv::application;
using namespace ksv::presentation;

namespace {
    class FakeCompletionHistoryUseCase final : public ICompletionHistoryUseCase {
    public:
        CompletionHistory history;

        CompletionHistory get_history() override { return history; }
        void onCurrentScenarioChanged(std::function<void()>) override {}
    };

    class CompletionHistoryViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeCompletionHistoryUseCase> fake = std::make_shared<FakeCompletionHistoryUseCase>();
        CompletionHistoryViewModel view_model{fake};

        void setHistory() const {
            fake->history = {
                .scenario_name = "Air Angelic",
                .rows = {
                    {.run_index = 1, .score = 100.0, .accuracy = 0.45, .shots = 20, .hits = 9, .misses = 11},
                    {.run_index = 2, .score = 125.0, .accuracy = 0.60, .shots = 25, .hits = 15, .misses = 10},
                },
            };
        }
    };

    TEST_F(CompletionHistoryViewModelTest, RefreshBuildsEverySeriesAgainstOneBasedRunIndices) {
        setHistory();

        view_model.refresh();

        const auto score = view_model.seriesPoints(CompletionHistoryViewModel::Score);
        const auto misses = view_model.seriesPoints(CompletionHistoryViewModel::Misses);
        ASSERT_EQ(score.size(), 2);
        ASSERT_EQ(misses.size(), 2);
        EXPECT_DOUBLE_EQ(score[0].x(), 1.0);
        EXPECT_DOUBLE_EQ(score[1].x(), 2.0);
        EXPECT_DOUBLE_EQ(score[1].y(), 125.0);
        EXPECT_DOUBLE_EQ(misses[0].y(), 11.0);
        EXPECT_EQ(view_model.runCount(), 2);
    }

    TEST_F(CompletionHistoryViewModelTest, RunAxisUsesWholeTicksAndRunLabels) {
        setHistory();
        view_model.refresh();

        for (const qreal tick: view_model.xAxis().ticks()) {
            EXPECT_DOUBLE_EQ(tick, std::round(tick));
        }
        EXPECT_EQ(view_model.xAxis().formatTick(2.0), "#2");
    }

    TEST_F(CompletionHistoryViewModelTest, AccuracyStaysRawAndFormatsAsPercentage) {
        setHistory();
        view_model.refresh();

        const auto accuracy_points = view_model.seriesPoints(CompletionHistoryViewModel::Accuracy);
        ASSERT_EQ(accuracy_points.size(), 2);
        EXPECT_DOUBLE_EQ(accuracy_points[0].y(), 0.45);

        const auto accuracy_series = view_model.series({CompletionHistoryViewModel::Accuracy});
        ASSERT_EQ(accuracy_series.size(), 1);
        EXPECT_EQ(accuracy_series.front()->formattedValueAtX(1.0), "45%");
    }

    TEST_F(CompletionHistoryViewModelTest, CountSeriesShareAnAxisButScoreDoesNot) {
        setHistory();
        view_model.refresh();

        const auto series = view_model.series({CompletionHistoryViewModel::Score,
                                                CompletionHistoryViewModel::Shots,
                                                CompletionHistoryViewModel::Hits});
        ASSERT_EQ(series.size(), 3);
        ASSERT_TRUE(series[0]->yAxis.has_value());
        ASSERT_TRUE(series[1]->yAxis.has_value());
        ASSERT_TRUE(series[2]->yAxis.has_value());
        EXPECT_DOUBLE_EQ(series[1]->yAxis->min(), series[2]->yAxis->min());
        EXPECT_DOUBLE_EQ(series[1]->yAxis->max(), series[2]->yAxis->max());
        EXPECT_NE(series[0]->yAxis->max(), series[1]->yAxis->max());
        EXPECT_LE(series[1]->yAxis->min(), 0.0);
        EXPECT_GE(series[1]->yAxis->max(), 25.0);
    }

    TEST_F(CompletionHistoryViewModelTest, LegacyAxesAreAvailableForEveryColumn) {
        setHistory();
        view_model.refresh();

        for (int column = CompletionHistoryViewModel::RunIndex;
             column < CompletionHistoryViewModel::ColumnCount; ++column) {
            const auto bounds = view_model.axisBounds()[QString::number(column)].toPointF();
            const auto ticks = view_model.axisTicks(column);
            EXPECT_LT(bounds.x(), bounds.y());
            EXPECT_GE(ticks.size(), 2);
        }
    }

    TEST_F(CompletionHistoryViewModelTest, RunIndexHasNoDrawableSeries) {
        setHistory();
        view_model.refresh();

        EXPECT_TRUE(view_model.series({CompletionHistoryViewModel::RunIndex}).isEmpty());
        EXPECT_TRUE(view_model.seriesPoints(CompletionHistoryViewModel::RunIndex).isEmpty());
    }

    TEST_F(CompletionHistoryViewModelTest, EmptyHistoryHasNonDegenerateAxes) {
        view_model.refresh();

        const auto bounds = view_model.axisBounds();
        for (int column = CompletionHistoryViewModel::RunIndex;
             column < CompletionHistoryViewModel::ColumnCount; ++column) {
            const QPointF range = bounds[QString::number(column)].toPointF();
            EXPECT_LT(range.x(), range.y());
        }
    }

    TEST_F(CompletionHistoryViewModelTest, RefreshAlwaysEmitsDataAndBoundsSignals) {
        const QSignalSpy data_spy(&view_model, &GraphViewModelBase::dataUpdated);
        const QSignalSpy bounds_spy(&view_model, &GraphViewModelBase::boundsChanged);

        view_model.refresh();

        EXPECT_EQ(data_spy.count(), 1);
        EXPECT_EQ(bounds_spy.count(), 1);
    }

    TEST_F(CompletionHistoryViewModelTest, ScenarioTitleOnlySignalsWhenItChanges) {
        setHistory();
        const QSignalSpy title_spy(&view_model, &CompletionHistoryViewModel::scenarioTitleChanged);

        view_model.refresh();
        view_model.refresh();

        EXPECT_EQ(view_model.scenarioTitle(), "Air Angelic");
        EXPECT_EQ(title_spy.count(), 1);
    }

    TEST_F(CompletionHistoryViewModelTest, EveryMetricHasANameColorAndUniqueId) {
        QSet<QString> ids;
        for (int column = CompletionHistoryViewModel::Score;
             column < CompletionHistoryViewModel::ColumnCount; ++column) {
            const auto series = view_model.series({column});
            ASSERT_EQ(series.size(), 1);
            EXPECT_FALSE(series.front()->name().isEmpty());
            EXPECT_TRUE(series.front()->color().isValid());
            EXPECT_FALSE(series.front()->id().isEmpty());
            ids.insert(series.front()->id());
        }
        EXPECT_EQ(ids.size(), CompletionHistoryViewModel::ColumnCount - CompletionHistoryViewModel::Score);
    }
}
