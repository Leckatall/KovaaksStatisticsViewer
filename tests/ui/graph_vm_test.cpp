//
// GraphViewModel tests using a hand-written fake IGraphUseCase.
//

#include <gtest/gtest.h>

#include <map>
#include <optional>

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

    SeriesPoints pts(const std::vector<double> &times, const std::vector<double> &ys) {
        SeriesPoints points;
        for (size_t i = 0; i < ys.size(); ++i) points.emplace_back(times[i], ys[i]);
        return points;
    }

    class FakeGraphUseCase : public IGraphUseCase {
    public:
        std::vector<std::string> load_perf_calls;
        int load_latest_perf_calls = 0;
        std::string run_label;

        std::vector<SeriesConfig> series_configs;
        std::map<uint64_t, std::optional<SeriesPoints>> series_values;
        std::vector<AxisConfig> axes;
        double run_duration = 0.0;
        bool has_current_run = false;
        bool has_performance = false;

        void load_perf(const std::string_view filename) override { load_perf_calls.emplace_back(filename); }
        void load_latest_perf() override { load_latest_perf_calls++; }
        std::string get_run_label() override { return run_label; }
        bool hasCurrentRun() const override { return has_current_run; }
        bool currentRunHasPerformance() const override { return has_performance; }
        void onCurrentPerfChanged(std::function<void()>) override {}
        void onSeriesConfigChanged(std::function<void()>) override {}

        std::vector<SeriesConfig> getSeriesConfigs() override { return series_configs; }
        std::optional<SeriesPoints> getSeriesValues(const SeriesId id) override {
            const auto it = series_values.find(id.value);
            return it == series_values.end() ? std::nullopt : it->second;
        }
        std::vector<AxisConfig> getAxes() override { return axes; }
        double getRunDuration() override { return run_duration; }

        void addSeries(const SeriesConfig &config, const SeriesPoints &points) {
            series_configs.push_back(config);
            series_values[config.id.value] = points;
        }
    };

    class GraphViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphUseCase> fake_use_case = std::make_shared<FakeGraphUseCase>();
        GraphViewModel view_model{fake_use_case};

        // One value per whole second, using the built-in Score/Accuracy SeriesConfigs.
        void setSampleData() {
            for (const auto &config : defaultSeriesConfigs()) {
                if (config.id.value == kScore) fake_use_case->addSeries(config, pts({0, 1, 2}, {10, 20, 30}));
                else if (config.id.value == kAccuracy) fake_use_case->addSeries(config, pts({0, 1, 2}, {0.5, 0.6, 0.7}));
            }
            fake_use_case->run_duration = 2.0;
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
        setSampleData();
        view_model.fetchData();

        EXPECT_DOUBLE_EQ(view_model.xAxis().min(), 0.0);
        EXPECT_DOUBLE_EQ(view_model.xAxis().max(), 2.0);
        EXPECT_EQ(view_model.xAxis().ticks(), (QList<qreal>{0.0, 1.0, 2.0}));
    }

    TEST_F(GraphViewModelTest, SeriesGroupsTheScoreFamilyOntoOneSharedAxis) {
        fake_use_case->axes = defaultAxisConfigs();
        for (const auto &config : defaultSeriesConfigs())
            if (config.id.value == kScoreTotal || config.id.value == kExpectedFinalScore)
                fake_use_case->addSeries(config, pts({0, 1}, {1, 2}));
        fake_use_case->run_duration = 1.0;
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

        fake_use_case->axes = {customAxis};
        fake_use_case->addSeries(first, pts({0, 1}, {2, 4}));
        fake_use_case->addSeries(second, pts({0, 1}, {10, 20}));
        fake_use_case->run_duration = 1.0;
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

        fake_use_case->axes = {sharedAxis};
        fake_use_case->addSeries(scoreTotal, pts({0, 1}, {5, 6}));
        fake_use_case->addSeries(computed, pts({0, 1}, {1, 1}));
        fake_use_case->run_duration = 1.0;
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

        fake_use_case->addSeries(first, pts({0, 1}, {2, 4}));
        fake_use_case->addSeries(second, pts({0, 1}, {100, 200}));
        fake_use_case->run_duration = 1.0;
        view_model.fetchMetadata();

        const auto series = view_model.series({10, 11});
        ASSERT_EQ(series.size(), 2);
        ASSERT_TRUE(series[0]->yAxis.has_value());
        ASSERT_TRUE(series[1]->yAxis.has_value());
        EXPECT_NE(series[0]->yAxis->max(), series[1]->yAxis->max());
    }

    TEST_F(GraphViewModelTest, SeriesTransformKindScalesDisplayedValues) {
        const SeriesConfig accuracy = defaultSeriesConfigs()[1];
        fake_use_case->addSeries(accuracy, pts({0}, {0.5}));
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

        fake_use_case->axes = {percentAxis};
        fake_use_case->addSeries(first, pts({0}, {0.5}));
        fake_use_case->addSeries(second, pts({0}, {0.5}));
        view_model.fetchMetadata();

        const auto series = view_model.series({10, 11});
        ASSERT_EQ(series.size(), 2);
        ASSERT_TRUE(series[0]->yAxis.has_value());
        EXPECT_EQ(series[0]->yAxis->formatTick(50.0), "50%");
    }

    TEST_F(GraphViewModelTest, FetchDataPopulatesShotsKillsAndDmgColumns) {
        for (const auto &config : defaultSeriesConfigs()) {
            if (config.id.value == kShots) fake_use_case->addSeries(config, pts({0, 1, 2}, {5, 10, 15}));
            else if (config.id.value == kKills) fake_use_case->addSeries(config, pts({0, 1, 2}, {1, 2, 3}));
            else if (config.id.value == kDmg) fake_use_case->addSeries(config, pts({0, 1, 2}, {100, 200, 300}));
        }
        fake_use_case->run_duration = 2.0;

        view_model.fetchData();

        EXPECT_EQ(view_model.series({kShots}).front()->points[0].y(), 5);
        EXPECT_EQ(view_model.series({kKills}).front()->points[1].y(), 2);
        EXPECT_EQ(view_model.series({kDmg}).front()->points[2].y(), 300.0);
    }

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
        view_model.fetchData();
        EXPECT_TRUE(view_model.enabledSeriesIds().isEmpty());
    }

    TEST_F(GraphViewModelTest, ResolvesColumnsForEnabledSeriesIds) {
        fake_use_case->addSeries(
            SeriesConfig{{1}, {"Score", {{0, 150, 0, 255}, 2.0}, true, 0}, primitive(PrimitiveMetric::Score)},
            pts({0, 1}, {10, 20}));
        fake_use_case->run_duration = 1.0;

        view_model.fetchData();

        EXPECT_EQ(view_model.enabledSeriesIds(), (QVariantList{"1"}));
        EXPECT_EQ(view_model.columnForSeriesId("1"), kScore);
        EXPECT_EQ(view_model.seriesIdForColumn(kScore), "1");
        EXPECT_EQ(view_model.columnForSeriesId("missing"), -1);
    }

    TEST_F(GraphViewModelTest, ContentStateStartsAtNoRunSelected) {
        EXPECT_EQ(view_model.contentState(), GraphViewModel::NoRunSelected);
    }

    TEST_F(GraphViewModelTest, ContentStateIsHasDataWhenRunHasPerformance) {
        setSampleData();
        fake_use_case->has_current_run = true;
        fake_use_case->has_performance = true;

        const QSignalSpy spy(&view_model, &GraphViewModel::contentStateChanged);
        view_model.fetchData();

        EXPECT_EQ(view_model.contentState(), GraphViewModel::HasData);
        EXPECT_EQ(spy.count(), 1);
    }

    TEST_F(GraphViewModelTest, ContentStateIsNoPerformanceForCsvOnlyRun) {
        fake_use_case->has_current_run = true;
        fake_use_case->has_performance = false;

        view_model.fetchData();

        EXPECT_EQ(view_model.contentState(), GraphViewModel::NoPerformanceData);
    }

    TEST_F(GraphViewModelTest, ContentStateReturnsToNoRunSelectedWhenRunCleared) {
        fake_use_case->has_current_run = true;
        fake_use_case->has_performance = true;
        view_model.fetchData();
        ASSERT_EQ(view_model.contentState(), GraphViewModel::HasData);

        fake_use_case->has_current_run = false;
        fake_use_case->has_performance = false;
        view_model.fetchData();

        EXPECT_EQ(view_model.contentState(), GraphViewModel::NoRunSelected);
    }

    TEST_F(GraphViewModelTest, ExpectedFinalScoreRecentRendersUnderItsOwnColumnDespiteHitsOccupyingASlot) {
        // Regression coverage: column is the series' own SeriesId, so a mid-list (disabled) Hits entry
        // can no longer shift later built-ins under each other's name/color.
        for (const auto &config : defaultSeriesConfigs())
            fake_use_case->addSeries(config, pts({0, 1}, {1, 2}));
        fake_use_case->run_duration = 1.0;

        view_model.fetchMetadata();

        EXPECT_EQ(view_model.columnForSeriesId("9"), kExpectedFinalScoreRecent);
        ASSERT_FALSE(view_model.series({kExpectedFinalScoreRecent}).isEmpty());
        EXPECT_EQ(view_model.series({kExpectedFinalScoreRecent}).front()->name(), "Expected Final Score (5s)");

        EXPECT_EQ(view_model.columnForSeriesId("5"), kKills);
        EXPECT_EQ(view_model.series({kKills}).front()->name(), "Kills");
    }
}
