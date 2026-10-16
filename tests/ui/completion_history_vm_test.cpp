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
        std::optional<int> selected;

        CompletionHistory get_history() override { return history; }
        void onCurrentScenarioChanged(std::function<void()>) override {}
        void onRangeChanged(std::function<void()>) override {}
        std::optional<int> selected_run_index() override { return selected; }
        void onSelectedRunChanged(std::function<void()>) override {}
        std::vector<int> select_run_calls;
        void select_run(const int run_index) override { select_run_calls.push_back(run_index); }
    };

    class CompletionHistoryViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeCompletionHistoryUseCase> fake = std::make_shared<FakeCompletionHistoryUseCase>();
        CompletionHistoryViewModel view_model{fake};

        // The use case hands over only the visible rows, each keeping its original run number.
        void project(const int fullCount, const std::vector<int> &visibleRuns) const {
            fake->history = {.scenario_name = "Air Angelic", .full_count = fullCount};
            for (const int run: visibleRuns)
                fake->history.rows.push_back({.run_index = run, .score = 100.0 + run, .accuracy = 0.5,
                                              .shots = 10, .hits = 5, .misses = 5});
        }

        void setHistory() const {
            fake->history = {
                .scenario_name = "Air Angelic",
                .full_count = 2,
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

        const auto score = view_model.series({CompletionHistoryViewModel::Score}).front()->points;
        const auto misses = view_model.series({CompletionHistoryViewModel::Misses}).front()->points;
        ASSERT_EQ(score.size(), 2);
        ASSERT_EQ(misses.size(), 2);
        EXPECT_DOUBLE_EQ(score[0].x(), 1.0);
        EXPECT_DOUBLE_EQ(score[1].x(), 2.0);
        EXPECT_DOUBLE_EQ(score[1].y(), 125.0);
        EXPECT_DOUBLE_EQ(misses[0].y(), 11.0);
        EXPECT_EQ(view_model.runCount(), 2);
    }

    TEST_F(CompletionHistoryViewModelTest, XAxisIdentityIsStableAcrossRefresh) {
        setHistory();
        const Axis *const before = &view_model.xAxis();

        view_model.refresh();

        EXPECT_EQ(&view_model.xAxis(), before);
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

        const auto accuracy_series = view_model.series({CompletionHistoryViewModel::Accuracy});
        ASSERT_EQ(accuracy_series.size(), 1);
        ASSERT_EQ(accuracy_series.front()->points.size(), 2);
        EXPECT_DOUBLE_EQ(accuracy_series.front()->points[0].y(), 0.45);
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

    TEST_F(CompletionHistoryViewModelTest, SeriesFollowThePlottableColumnOrder) {
        setHistory();
        view_model.refresh();

        QList<int> columns;
        for (const auto *s: view_model.series({CompletionHistoryViewModel::Misses, CompletionHistoryViewModel::Score,
                                               CompletionHistoryViewModel::Accuracy}))
            columns.append(s->column());
        EXPECT_EQ(columns, (QList<int>{CompletionHistoryViewModel::Score, CompletionHistoryViewModel::Accuracy,
                                       CompletionHistoryViewModel::Misses}));
    }

    TEST_F(CompletionHistoryViewModelTest, RunIndexHasNoDrawableSeries) {
        setHistory();
        view_model.refresh();

        EXPECT_TRUE(view_model.series({CompletionHistoryViewModel::RunIndex}).isEmpty());
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

    // The run axis must only label runs that exist: no "#0", nothing past the latest run.
    TEST_F(CompletionHistoryViewModelTest, RunAxisLabelsOnlyRunsThatExist) {
        for (const int runs: {2, 4, 8, 224, 360}) {
            fake->history.rows.clear();
            fake->history.full_count = runs;
            for (int i = 1; i <= runs; ++i)
                fake->history.rows.push_back({.run_index = i, .score = 100.0 + i, .accuracy = 0.5,
                                              .shots = 10, .hits = 5, .misses = 5});
            view_model.refresh();

            const auto &axis = view_model.xAxis();
            ASSERT_FALSE(axis.ticks().isEmpty());
            EXPECT_DOUBLE_EQ(axis.min(), 1.0) << runs << " runs";
            EXPECT_DOUBLE_EQ(axis.max(), runs) << runs << " runs";
            for (const qreal tick: axis.ticks()) {
                EXPECT_GE(tick, 1.0) << runs << " runs: tick " << axis.formatTick(tick).toStdString();
                EXPECT_LE(tick, runs) << runs << " runs: tick " << axis.formatTick(tick).toStdString();
            }
        }
    }

    TEST_F(CompletionHistoryViewModelTest, FewRunsAreEachTickedFromTheFirstRun) {
        fake->history.rows.clear();
        fake->history.full_count = 8;
        for (int i = 1; i <= 8; ++i)
            fake->history.rows.push_back({.run_index = i, .score = 3100.0 + 40 * i});
        view_model.refresh();

        EXPECT_EQ(view_model.xAxis().ticks(), (QList<qreal>{1, 2, 3, 4, 5, 6, 7, 8}));
        EXPECT_EQ(view_model.xAxis().formatTick(view_model.xAxis().ticks().front()), "#1");
    }

    // The history graph marks the selected run at its run number; -1 marks nothing.
    TEST_F(CompletionHistoryViewModelTest, SelectedRunIndexFollowsTheUseCase) {
        setHistory();
        view_model.refresh();
        EXPECT_EQ(view_model.selectedRunIndex(), -1);

        const QSignalSpy spy(&view_model, &CompletionHistoryViewModel::selectedRunIndexChanged);
        fake->selected = 2;
        view_model.refreshSelection();
        EXPECT_EQ(view_model.selectedRunIndex(), 2);
        view_model.refreshSelection();
        EXPECT_EQ(spy.count(), 1) << "an unchanged selection does not signal";

        fake->selected = std::nullopt;
        view_model.refresh();
        EXPECT_EQ(view_model.selectedRunIndex(), -1);
        EXPECT_EQ(spy.count(), 2);
    }

    TEST_F(CompletionHistoryViewModelTest, SelectRunForwardsOnlyRunsOnTheGraph) {
        setHistory();
        view_model.refresh();

        view_model.selectRun(0);
        view_model.selectRun(3);
        view_model.selectRun(2);

        EXPECT_EQ(fake->select_run_calls, std::vector<int>{2});
    }

    TEST_F(CompletionHistoryViewModelTest, CountSeriesShareOneAxisIdentity) {
        const auto series = view_model.series({CompletionHistoryViewModel::Score, CompletionHistoryViewModel::Shots,
                                               CompletionHistoryViewModel::Hits});
        ASSERT_EQ(series.size(), 3);
        ASSERT_TRUE(series[0]->yAxisId && series[1]->yAxisId && series[2]->yAxisId);
        EXPECT_NE(*series[0]->yAxisId, *series[1]->yAxisId);
        EXPECT_EQ(*series[1]->yAxisId, *series[2]->yAxisId);
    }

    TEST_F(CompletionHistoryViewModelTest, ProjectedRowsKeepTheirOriginalRunNumbersAsX) {
        project(5, {4, 5});

        view_model.refresh();

        const auto score = view_model.series({CompletionHistoryViewModel::Score}).front()->points;
        ASSERT_EQ(score.size(), 2);
        EXPECT_DOUBLE_EQ(score[0].x(), 4.0);
        EXPECT_DOUBLE_EQ(score[1].x(), 5.0);
        EXPECT_DOUBLE_EQ(score[0].y(), 104.0);
    }

    TEST_F(CompletionHistoryViewModelTest, FullAndVisibleCountsAreDistinct) {
        project(5, {4, 5});

        view_model.refresh();

        EXPECT_EQ(view_model.runCount(), 5);
        EXPECT_EQ(view_model.visibleRunCount(), 2);
    }

    TEST_F(CompletionHistoryViewModelTest, XAxisSpansTheOriginalVisibleRunNumbers) {
        project(40, {31, 32, 33, 34, 35, 36, 37, 38, 39, 40});

        view_model.refresh();

        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), 31.0);
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), 40.0);
        for (const qreal tick: view_model.xAxis().ticks()) {
            EXPECT_GE(tick, 31.0);
            EXPECT_LE(tick, 40.0);
        }
        EXPECT_EQ(view_model.xAxis().formatTick(31.0), "#31");
    }

    TEST_F(CompletionHistoryViewModelTest, AllHistoryAxisIsUnchangedFromOneToTheLastRun) {
        setHistory();

        view_model.refresh();

        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), 1.0);
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), 2.0);
        EXPECT_EQ(view_model.visibleRunCount(), view_model.runCount());
    }

    // A one-run chart still needs an axis that labels the run and no run that does not exist.
    TEST_F(CompletionHistoryViewModelTest, OneVisibleRowGetsAnAxisLabellingOnlyThatRun) {
        project(9, {7});

        view_model.refresh();

        EXPECT_EQ(view_model.visibleRunCount(), 1);
        EXPECT_EQ(view_model.xAxis().ticks(), (QList<qreal>{7}));
        EXPECT_EQ(view_model.xAxis().formatTick(7.0), "#7");
        EXPECT_GT(view_model.xAxis().normalizedPosition(7.0), 0.0);
        EXPECT_LT(view_model.xAxis().normalizedPosition(7.0), 1.0);
        ASSERT_EQ(view_model.series({CompletionHistoryViewModel::Score}).front()->points.size(), 1);
    }

    TEST_F(CompletionHistoryViewModelTest, OnlyRunOneGetsAnAxisThatNeverLabelsRunZero) {
        project(1, {1});

        view_model.refresh();

        EXPECT_EQ(view_model.xAxis().ticks(), (QList<qreal>{1}));
    }

    TEST_F(CompletionHistoryViewModelTest, EmptyProjectionKeepsTheHistoryCountAndDrawsNothing) {
        project(6, {});

        view_model.refresh();

        EXPECT_EQ(view_model.runCount(), 6);
        EXPECT_EQ(view_model.visibleRunCount(), 0);
        EXPECT_TRUE(view_model.series({CompletionHistoryViewModel::Score}).front()->points.isEmpty());
        EXPECT_GT(view_model.xAxis().max(), view_model.xAxis().min());
    }

    TEST_F(CompletionHistoryViewModelTest, NoHistoryHasNeitherFullNorVisibleRuns) {
        view_model.refresh();

        EXPECT_EQ(view_model.runCount(), 0);
        EXPECT_EQ(view_model.visibleRunCount(), 0);
    }

    TEST_F(CompletionHistoryViewModelTest, SelectedRunInsideTheProjectionIsMarkable) {
        project(5, {4, 5});
        fake->selected = 5;

        view_model.refresh();

        EXPECT_EQ(view_model.selectedRunIndex(), 5);
        EXPECT_TRUE(view_model.selectedRunVisible());
    }

    TEST_F(CompletionHistoryViewModelTest, SelectedRunOutsideTheProjectionStaysSelectedButIsNotMarkable) {
        project(5, {4, 5});
        fake->selected = 2;

        view_model.refresh();

        EXPECT_EQ(view_model.selectedRunIndex(), 2) << "the run is still the selected one";
        EXPECT_FALSE(view_model.selectedRunVisible());
    }

    TEST_F(CompletionHistoryViewModelTest, NoSelectedRunIsNotVisible) {
        setHistory();

        view_model.refresh();

        EXPECT_FALSE(view_model.selectedRunVisible());
    }

    TEST_F(CompletionHistoryViewModelTest, WideningTheRangeRevealsAnAlreadySelectedRunWithoutReselecting) {
        project(5, {4, 5});
        fake->selected = 2;
        view_model.refresh();
        const QSignalSpy visibleSpy(&view_model, &CompletionHistoryViewModel::selectedRunVisibleChanged);

        project(5, {1, 2, 3, 4, 5});
        view_model.refresh();

        EXPECT_TRUE(view_model.selectedRunVisible());
        EXPECT_EQ(view_model.selectedRunIndex(), 2);
        EXPECT_EQ(visibleSpy.count(), 1);
        EXPECT_TRUE(fake->select_run_calls.empty());
    }

    TEST_F(CompletionHistoryViewModelTest, SelectRunForwardsTheOriginalNumberOfAVisibleRun) {
        project(5, {4, 5});
        view_model.refresh();

        view_model.selectRun(5);

        EXPECT_EQ(fake->select_run_calls, std::vector<int>{5});
    }

    TEST_F(CompletionHistoryViewModelTest, SelectRunIgnoresRunsOutsideTheProjection) {
        project(5, {4, 5});
        view_model.refresh();

        view_model.selectRun(1);
        view_model.selectRun(3);
        view_model.selectRun(6);

        EXPECT_TRUE(fake->select_run_calls.empty());
    }

    TEST_F(CompletionHistoryViewModelTest, TooltipValuesAddressTheOriginalRows) {
        project(5, {4, 5});
        view_model.refresh();

        const auto sample = view_model.series({CompletionHistoryViewModel::Score}).front()->sampleAtX(5.0);

        ASSERT_TRUE(sample.has_value());
        EXPECT_DOUBLE_EQ(sample->x(), 5.0);
        EXPECT_DOUBLE_EQ(sample->y(), 105.0);
    }
}
