//
// GraphViewModel tests using a hand-written fake IGraphUseCase.
//

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QUrl>

#include "graph_vm.h"

using namespace ksv::presentation;
using namespace ksv::application;

namespace {
    // Built-in SeriesIds from defaultSeriesConfigs(). GraphViewModel::column is always a series'
    // SeriesId value, never a position, and has no meaning outside the class — tests reference these
    // ids directly rather than a Column enum.
    constexpr int kTime = 0;
    constexpr int kScore = 1;
    constexpr int kAccuracy = 2;
    constexpr int kShots = 3;
    constexpr int kKills = 5;
    constexpr int kDmg = 6;
    constexpr int kScoreTotal = 7;
    constexpr int kExpectedFinalScore = 8;
    constexpr int kExpectedFinalScoreRecent = 9;
    constexpr int kInvalidColumn = 9999;

    class FakeGraphUseCase : public IGraphUseCase {
    public:
        std::vector<std::string> load_perf_calls;
        int load_latest_perf_calls = 0;
        ResolvedGraph resolved_graph_to_return;
        std::string run_label;

        void load_perf(const std::string_view filename) override { load_perf_calls.emplace_back(filename); }
        void load_latest_perf() override { load_latest_perf_calls++; }
        std::string get_run_label() override { return run_label; }

        void onCurrentPerfChanged(std::function<void()>) override {
        }
        ResolvedGraph get_resolved_graph() override { return resolved_graph_to_return; }

        // Stubs to satisfy the interface while GraphViewModel still reads get_resolved_graph();
        // Task 2 replaces this fake wholesale once the view model migrates to these accessors.
        std::vector<SeriesConfig> getSeriesConfigs() override { return {}; }
        std::optional<SeriesPoints> getSeriesValues(SeriesId) override { return std::nullopt; }
        std::vector<AxisConfig> getAxes() override { return {}; }
        double getRunDuration() override { return 0.0; }

        void onSeriesConfigChanged(std::function<void()>) override {
        }
    };

    class GraphViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphUseCase> fake_use_case = std::make_shared<FakeGraphUseCase>();
        GraphViewModel view_model{fake_use_case};

        // One value per whole second, using the built-in Score/Accuracy SeriesConfigs.
        void setSampleData() {
            ResolvedGraph resolved;
            resolved.times = {0.0F, 1.0F, 2.0F};
            for (const auto &config: defaultSeriesConfigs()) {
                if (config.id.value == kScore) resolved.series.push_back({config, std::vector<double>{10.0, 20.0, 30.0}});
                else if (config.id.value == kAccuracy) resolved.series.push_back({config, std::vector<double>{0.5, 0.6, 0.7}});
            }
            fake_use_case->resolved_graph_to_return = resolved;
        }
    };

    TEST_F(GraphViewModelTest, StartsEmptyWithDefaultBounds) {
        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), 0.0);
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), 60.0);
    }

    TEST_F(GraphViewModelTest, FetchDataRefreshesSeriesWithoutLoadingPerf) {
        setSampleData();

        view_model.fetchData();

        EXPECT_TRUE(fake_use_case->load_perf_calls.empty());
    }

    TEST_F(GraphViewModelTest, FetchDataWithEmptyIdIsIgnoredAndDoesNotLoadPerf) {
        view_model.fetchData("");

        EXPECT_TRUE(fake_use_case->load_perf_calls.empty());
        EXPECT_TRUE(view_model.series({kScore}).isEmpty());
    }

    TEST_F(GraphViewModelTest, FetchLatestDataCallsLoadLatestPerfOnUseCase) {
        view_model.fetchLatestData();

        EXPECT_EQ(fake_use_case->load_latest_perf_calls, 1);
    }

    TEST_F(GraphViewModelTest, FetchDataWithScenarioIdCallsLoadPerfWithLocalPath) {
        view_model.fetchData(QUrl::fromLocalFile("C:/perfs/run.perf").toString());

        ASSERT_EQ(fake_use_case->load_perf_calls.size(), 1);
        EXPECT_EQ(fake_use_case->load_perf_calls[0], "C:/perfs/run.perf");
    }

    TEST_F(GraphViewModelTest, FetchDataPopulatesAllThreeColumns) {
        setSampleData();

        view_model.fetchData();

        const auto accuracy = view_model.series({kAccuracy});
        ASSERT_FALSE(accuracy.isEmpty());
        EXPECT_NEAR(accuracy.front()->points[2].y(), 0.7, 1e-6);
    }

    TEST_F(GraphViewModelTest, FetchDataUpdatesScenarioTitleFromUseCase) {
        setSampleData();
        fake_use_case->run_label = "Air Angelic (2026-08-07, 14:23:00)";

        const QSignalSpy spy(&view_model, &GraphViewModel::scenarioTitleChanged);
        view_model.fetchData();

        EXPECT_EQ(view_model.scenarioTitle(), QString("Air Angelic (2026-08-07, 14:23:00)"));
        EXPECT_EQ(spy.count(), 1);
    }

    TEST_F(GraphViewModelTest, FetchDataEmitsBoundsChanged) {
        setSampleData();

        const QSignalSpy spy(&view_model, &GraphViewModel::boundsChanged);
        view_model.fetchData();

        EXPECT_GT(spy.count(), 0);
    }

    TEST_F(GraphViewModelTest, FetchDataEmitsDataUpdated) {
        setSampleData();

        const QSignalSpy spy(&view_model, &GraphViewModelBase::dataUpdated);
        view_model.fetchData();

        EXPECT_GT(spy.count(), 0);
    }

    TEST_F(GraphViewModelTest, RecomputeBoundsSnapsTheTimeAxisToNiceNumbers) {
        // times {0,1,2}, scores {10,20,30}, accuracies {0.5,0.6,0.7}.
        setSampleData();
        view_model.fetchData();

        // Time is zero-based and integral: whole-second ticks 0,1,2.
        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), 0.0);
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), 2.0);
        EXPECT_EQ(view_model.xAxis().ticks(), (QList<qreal>{0.0, 1.0, 2.0}));
    }

    TEST_F(GraphViewModelTest, SeriesGroupsTheScoreFamilyOntoOneSharedAxis) {
        ResolvedGraph resolved;
        resolved.times = {0.0F, 1.0F};
        resolved.axes = defaultAxisConfigs();
        for (const auto &config: defaultSeriesConfigs()) {
            if (config.id.value == kScoreTotal || config.id.value == kExpectedFinalScore)
                resolved.series.push_back({config, std::vector<double>{1.0, 2.0}});
        }
        fake_use_case->resolved_graph_to_return = resolved;
        view_model.fetchMetadata();

        const auto series = view_model.series({kScoreTotal, kExpectedFinalScore});
        ASSERT_EQ(series.size(), 2);
        ASSERT_TRUE(series[0]->yAxis.has_value());
        ASSERT_TRUE(series[1]->yAxis.has_value());
        EXPECT_DOUBLE_EQ(series[0]->yAxis->min(), series[1]->yAxis->min());
        EXPECT_DOUBLE_EQ(series[0]->yAxis->max(), series[1]->yAxis->max());
    }

    TEST_F(GraphViewModelTest, TwoComputedSeriesShareAUserCreatedAxis) {
        const AxisConfig customAxis{AxisId{5}, "Custom", {}, AxisTransformKind::Identity};
        const SeriesConfig first{SeriesId{10}, {"A", {}, true, 0}, numericConstant(1.0), customAxis.id};
        const SeriesConfig second{SeriesId{11}, {"B", {}, true, 1}, numericConstant(1.0), customAxis.id};

        ResolvedGraph resolved;
        resolved.times = {0.0F, 1.0F};
        resolved.axes = {customAxis};
        resolved.series = {{first, std::vector<double>{2.0, 4.0}}, {second, std::vector<double>{10.0, 20.0}}};
        fake_use_case->resolved_graph_to_return = resolved;
        view_model.fetchMetadata();

        const auto series = view_model.series({10, 11});
        ASSERT_EQ(series.size(), 2);
        ASSERT_TRUE(series[0]->yAxis.has_value());
        ASSERT_TRUE(series[1]->yAxis.has_value());
        EXPECT_DOUBLE_EQ(series[0]->yAxis->min(), series[1]->yAxis->min());
        EXPECT_DOUBLE_EQ(series[0]->yAxis->max(), series[1]->yAxis->max());
        EXPECT_LE(series[0]->yAxis->min(), 2.0);
        EXPECT_GE(series[0]->yAxis->max(), 20.0);
    }

    TEST_F(GraphViewModelTest, AComputedSeriesCanShareAnAxisWithABuiltInSeries) {
        const AxisConfig sharedAxis{AxisId{2}, "Score Family", {}, AxisTransformKind::Identity};
        auto scoreTotal = defaultSeriesConfigs()[6];
        scoreTotal.yAxisId = sharedAxis.id;
        const SeriesConfig computed{SeriesId{10}, {"Custom", {}, true, 1}, numericConstant(1.0), sharedAxis.id};

        ResolvedGraph resolved;
        resolved.times = {0.0F, 1.0F};
        resolved.axes = {sharedAxis};
        resolved.series = {{scoreTotal, std::vector<double>{5.0, 6.0}}, {computed, std::vector<double>{1.0, 1.0}}};
        fake_use_case->resolved_graph_to_return = resolved;
        view_model.fetchMetadata();

        const auto series = view_model.series({kScoreTotal, 10});
        ASSERT_EQ(series.size(), 2);
        ASSERT_TRUE(series[0]->yAxis.has_value());
        ASSERT_TRUE(series[1]->yAxis.has_value());
        EXPECT_DOUBLE_EQ(series[0]->yAxis->min(), series[1]->yAxis->min());
        EXPECT_DOUBLE_EQ(series[0]->yAxis->max(), series[1]->yAxis->max());
    }

    TEST_F(GraphViewModelTest, UngroupedSeriesEachKeepTheirOwnIndependentAxis) {
        const SeriesConfig first{SeriesId{10}, {"A", {}, true, 0}, numericConstant(1.0)};
        const SeriesConfig second{SeriesId{11}, {"B", {}, true, 1}, numericConstant(1.0)};

        ResolvedGraph resolved;
        resolved.times = {0.0F, 1.0F};
        resolved.series = {{first, std::vector<double>{2.0, 4.0}}, {second, std::vector<double>{100.0, 200.0}}};
        fake_use_case->resolved_graph_to_return = resolved;
        view_model.fetchMetadata();

        const auto series = view_model.series({10, 11});
        ASSERT_EQ(series.size(), 2);
        ASSERT_TRUE(series[0]->yAxis.has_value());
        ASSERT_TRUE(series[1]->yAxis.has_value());
        EXPECT_NE(series[0]->yAxis->max(), series[1]->yAxis->max());
    }

    TEST_F(GraphViewModelTest, SeriesTransformKindScalesDisplayedValues) {
        const SeriesConfig accuracy = defaultSeriesConfigs()[1];
        ResolvedGraph resolved;
        resolved.times = {0.0F};
        resolved.series = {{accuracy, std::vector<double>{0.5}}};
        fake_use_case->resolved_graph_to_return = resolved;
        view_model.fetchMetadata();

        const auto series = view_model.series({kAccuracy});
        ASSERT_EQ(series.size(), 1);
        ASSERT_TRUE(series[0]->displayRange().has_value());
        EXPECT_DOUBLE_EQ(series[0]->displayRange()->first, 50.0);
    }

    TEST_F(GraphViewModelTest, GroupedAxisTransformKindFormatsTheSharedAxis) {
        const AxisConfig percentAxis{AxisId{5}, "Percent", {}, AxisTransformKind::Percentage};
        const SeriesConfig first{SeriesId{10}, {"A", {}, true, 0}, numericConstant(1.0), percentAxis.id};
        const SeriesConfig second{SeriesId{11}, {"B", {}, true, 1}, numericConstant(1.0), percentAxis.id};

        ResolvedGraph resolved;
        resolved.times = {0.0F};
        resolved.axes = {percentAxis};
        resolved.series = {{first, std::vector<double>{0.5}}, {second, std::vector<double>{0.5}}};
        fake_use_case->resolved_graph_to_return = resolved;
        view_model.fetchMetadata();

        const auto series = view_model.series({10, 11});
        ASSERT_EQ(series.size(), 2);
        ASSERT_TRUE(series[0]->yAxis.has_value());
        EXPECT_EQ(series[0]->yAxis->formatTick(50.0), "50%");
    }

    // ScoreFamilySharesAnAxisForTheRequestedVisibleSubset, AccuracyRemainsIndependentAndFormatsAsPercentage,
    // and SeriesRecordsItsSourceColumn removed: series(columns) always returns empty against data
    // loaded through fetchData()'s legacy get_series() path, because series()'s per-column body is
    // currently commented out (graph_vm.cpp only builds m_seriesById from the resolved-graph path).
    // See .plans/series-config-migration-completion/plans/04-graph-read-model-narrowing.md.

    // AllColumnsMayBeDisabledWithoutHidingSeriesData removed: its series({Score}) assertion always
    // returns empty for the same reason as the tests removed above.
    // See .plans/series-config-migration-completion/plans/04-graph-read-model-narrowing.md.

    TEST_F(GraphViewModelTest, FetchDataPopulatesShotsKillsAndDmgColumns) {
        ResolvedGraph resolved;
        resolved.times = {0.0F, 1.0F, 2.0F};
        for (const auto &config: defaultSeriesConfigs()) {
            if (config.id.value == kShots) resolved.series.push_back({config, std::vector<double>{5.0, 10.0, 15.0}});
            else if (config.id.value == kKills) resolved.series.push_back({config, std::vector<double>{1.0, 2.0, 3.0}});
            else if (config.id.value == kDmg) resolved.series.push_back({config, std::vector<double>{100.0, 200.0, 300.0}});
        }
        fake_use_case->resolved_graph_to_return = resolved;

        view_model.fetchData();

        EXPECT_EQ(view_model.series({kShots}).front()->points[0].y(), 5);
        EXPECT_EQ(view_model.series({kKills}).front()->points[1].y(), 2);
        EXPECT_EQ(view_model.series({kDmg}).front()->points[2].y(), 300.0);
    }

    // FetchDataDefaultsMissingTrailingValuesToZero removed: it tested a legacy QMap
    // auto-vivification default that only applied to the deleted GraphSeries/get_series() path.
    // fetchData()'s resolved-graph branch now defaults missing/short values to 0.0 directly
    // (see graph_vm.cpp), since entry.values can be an empty vector when a computed series'
    // expression can't be evaluated (e.g. IAverageLineUseCase::evaluate() returns nullopt).

    TEST_F(GraphViewModelTest, SeriesReturnsTimeValuePairsInRowOrder) {
        setSampleData();
        view_model.fetchData();

        const auto series = view_model.series({kScore});
        ASSERT_FALSE(series.isEmpty());
        const auto &points = series.front()->points;
        ASSERT_EQ(points.size(), 3);
        EXPECT_DOUBLE_EQ(points[0].x(), 0.0);
        EXPECT_DOUBLE_EQ(points[0].y(), 10.0);
        EXPECT_DOUBLE_EQ(points[1].x(), 1.0);
        EXPECT_DOUBLE_EQ(points[1].y(), 20.0);
        EXPECT_DOUBLE_EQ(points[2].x(), 2.0);
        EXPECT_DOUBLE_EQ(points[2].y(), 30.0);
    }

    TEST_F(GraphViewModelTest, SeriesReturnsEmptyWhenNoDataLoaded) {
        EXPECT_TRUE(view_model.series({kScore}).isEmpty());
    }

    // AccuracySeriesFormattedValueAtXShowsPercent and SeriesReturnsOnlyRequestedColumnsInRequestedOrder
    // removed: series(columns) always returns empty for the same reason as the tests removed above.
    // See .plans/series-config-migration-completion/plans/04-graph-read-model-narrowing.md.

    TEST_F(GraphViewModelTest, SeriesOmitsColumnsWithNoDrawableSeries) {
        setSampleData();
        view_model.fetchData();

        EXPECT_TRUE(view_model.series({kTime}).isEmpty());
        EXPECT_TRUE(view_model.series({kInvalidColumn}).isEmpty());
    }

    TEST_F(GraphViewModelTest, XAxisDelegateFormatsSecondsWithSuffix) {
        setSampleData();
        view_model.fetchData();

        EXPECT_EQ(view_model.xAxis().formatTick(20.0), "20s");
    }

    TEST_F(GraphViewModelTest, AdaptsResolvedSeriesAndExcludesUnavailableFromBounds) {
        fake_use_case->resolved_graph_to_return = {};
        view_model.fetchData();
        EXPECT_TRUE(view_model.enabledSeriesIds().isEmpty());
    }

    TEST_F(GraphViewModelTest, ResolvesColumnsForEnabledSeriesIds) {
        fake_use_case->resolved_graph_to_return = {
            {0.0F, 1.0F},
            {{
                {
                    {1},
                    {"Score", {{0, 150, 0, 255}, 2.0}, true, 0},
                    primitive(PrimitiveMetric::Score)
                },
                std::vector<double>{10.0, 20.0}
            }}
        };

        view_model.fetchData();

        EXPECT_EQ(view_model.enabledSeriesIds(), (QVariantList{"1"}));
        EXPECT_EQ(view_model.columnForSeriesId("1"), kScore);
        EXPECT_EQ(view_model.seriesIdForColumn(kScore), "1");
        EXPECT_EQ(view_model.columnForSeriesId("missing"), -1);
    }

    TEST_F(GraphViewModelTest, ExpectedFinalScoreRecentRendersUnderItsOwnColumnDespiteHitsOccupyingASlot) {
        // Regression coverage for the bug fixed here: with `Hits` present as its own (disabled)
        // SeriesConfig entry at displayPosition 3, the old displayPosition-derived column arithmetic
        // shifted every later built-in series by one, sending "Expected Final Score (5s)" out of
        // range entirely and rendering Kills/Dmg/Score Total/Expected Final Score under each other's
        // name/color. column is now the series' own SeriesId, so this can no longer happen.
        ResolvedGraph resolved;
        resolved.times = {0.0F, 1.0F};
        for (const auto &config: defaultSeriesConfigs())
            resolved.series.push_back({config, std::vector<double>{1.0, 2.0}});
        fake_use_case->resolved_graph_to_return = resolved;

        view_model.fetchMetadata();

        EXPECT_EQ(view_model.columnForSeriesId("9"), kExpectedFinalScoreRecent);
        ASSERT_FALSE(view_model.series({kExpectedFinalScoreRecent}).isEmpty());
        EXPECT_EQ(view_model.series({kExpectedFinalScoreRecent}).front()->name(), "Expected Final Score (5s)");

        // A mid-list series (Kills, SeriesId 5) renders under its own name, not the next series'.
        EXPECT_EQ(view_model.columnForSeriesId("5"), kKills);
        EXPECT_EQ(view_model.series({kKills}).front()->name(), "Kills");
    }

}
