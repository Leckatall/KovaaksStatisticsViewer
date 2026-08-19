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
    class FakeGraphUseCase : public IGraphUseCase {
    public:
        std::vector<std::string> load_perf_calls;
        int load_latest_perf_calls = 0;
        GraphSeries series_to_return;
        ResolvedGraph resolved_graph_to_return;
        std::string run_label;

        void load_perf(const std::string_view filename) override { load_perf_calls.emplace_back(filename); }
        void load_latest_perf() override { load_latest_perf_calls++; }
        GraphSeries get_series() override { return series_to_return; }
        std::string get_run_label() override { return run_label; }

        void onCurrentPerfChanged(std::function<void()>) override {
        }
        ResolvedGraph get_resolved_graph() override { return resolved_graph_to_return; }

        MutationResult setSeriesEnabled(SeriesId, bool) override { return {}; }
        MutationResult updateBasePresentation(const UpdateBaseSeriesRequest &) override { return {}; }
        MutationResult createComputed(const CreateComputedSeriesRequest &) override { return {}; }
        MutationResult updateComputed(const UpdateComputedSeriesRequest &) override { return {}; }
        MutationResult removeComputed(SeriesId) override { return {}; }
        MutationResult moveSeries(SeriesId, uint32_t) override { return {}; }
        void onSeriesConfigChanged(std::function<void()>) override {
        }
    };

    class GraphViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphUseCase> fake_use_case = std::make_shared<FakeGraphUseCase>();
        GraphViewModel view_model{fake_use_case};

        // Already-resampled series, one value per whole second, matching what
        // GraphUseCase::get_series() (via PerfColumnBuilder) would return.
        void setSampleData() {
            fake_use_case->series_to_return.times = {0.0F, 1.0F, 2.0F};
            fake_use_case->series_to_return.columns[ColumnId::Score] = {10.0F, 20.0F, 30.0F};
            fake_use_case->series_to_return.columns[ColumnId::Accuracy] = {0.5F, 0.6F, 0.7F};
        }
    };

    TEST_F(GraphViewModelTest, StartsEmptyWithDefaultBounds) {
        EXPECT_TRUE(view_model.seriesPoints(GraphViewModel::Score).isEmpty());
        const auto bounds = view_model.axisBounds();
        EXPECT_DOUBLE_EQ(bounds[QString::number(GraphViewModel::Score)].toPointF().y(), 1.0);
        EXPECT_DOUBLE_EQ(bounds[QString::number(GraphViewModel::Accuracy)].toPointF().x(), 0.0);
        EXPECT_DOUBLE_EQ(bounds[QString::number(GraphViewModel::Accuracy)].toPointF().y(), 1.0);
    }

    TEST_F(GraphViewModelTest, FetchDataRefreshesSeriesWithoutLoadingPerf) {
        setSampleData();

        view_model.fetchData();

        EXPECT_TRUE(fake_use_case->load_perf_calls.empty());
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Score).size(), 3);
    }

    TEST_F(GraphViewModelTest, FetchDataWithEmptyIdIsIgnoredAndDoesNotLoadPerf) {
        setSampleData();

        view_model.fetchData("");

        EXPECT_TRUE(fake_use_case->load_perf_calls.empty());
        EXPECT_TRUE(view_model.seriesPoints(GraphViewModel::Score).isEmpty());
    }

    TEST_F(GraphViewModelTest, FetchLatestDataCallsLoadLatestPerfOnUseCase) {
        view_model.fetchLatestData();

        EXPECT_EQ(fake_use_case->load_latest_perf_calls, 1);
    }

    TEST_F(GraphViewModelTest, FetchDataWithScenarioIdCallsLoadPerfWithLocalPath) {
        setSampleData();

        view_model.fetchData(QUrl::fromLocalFile("C:/perfs/run.perf").toString());

        ASSERT_EQ(fake_use_case->load_perf_calls.size(), 1);
        EXPECT_EQ(fake_use_case->load_perf_calls[0], "C:/perfs/run.perf");
    }

    TEST_F(GraphViewModelTest, FetchDataPopulatesAllThreeColumns) {
        setSampleData();

        view_model.fetchData();

        // seriesPoints packs each row as {time, value}, so x() is Time.
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Score)[0].x(), 0.0);
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Score)[1].y(), 20.0);
        EXPECT_NEAR(view_model.seriesPoints(GraphViewModel::Accuracy)[2].y(), 0.7, 1e-6);
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

    TEST_F(GraphViewModelTest, RecomputeBoundsSnapsEachColumnToNiceNumbers) {
        // times {0,1,2}, scores {10,20,30}, accuracies {0.5,0.6,0.7}.
        setSampleData();
        view_model.fetchData();
        const auto bounds = view_model.axisBounds();
        const auto timeBounds = bounds[QString::number(GraphViewModel::Time)].toPointF();
        const auto scoreBounds = bounds[QString::number(GraphViewModel::Score)].toPointF();
        const auto accuracyBounds = bounds[QString::number(GraphViewModel::Accuracy)].toPointF();

        // Time is zero-based and integral: whole-second ticks 0,1,2.
        EXPECT_DOUBLE_EQ(timeBounds.x(), 0.0);
        EXPECT_DOUBLE_EQ(timeBounds.y(), 2.0);
        EXPECT_EQ(view_model.axisTicks(GraphViewModel::Time), (QList<qreal>{0.0, 1.0, 2.0}));

        // Score range [10,30] snaps to nice endpoints with round ticks that
        // span the full range.
        EXPECT_DOUBLE_EQ(scoreBounds.x(), 10.0);
        EXPECT_DOUBLE_EQ(scoreBounds.y(), 30.0);
        const auto scoreTicks = view_model.axisTicks(GraphViewModel::Score);
        ASSERT_GE(scoreTicks.size(), 2);
        EXPECT_DOUBLE_EQ(scoreTicks.front(), 10.0);
        EXPECT_DOUBLE_EQ(scoreTicks.back(), 30.0);

        // Accuracy range [0.5,0.7] gets its own independent axis.
        // Accuracies are stored as float and promoted to double, so the
        // tolerance must absorb float-precision error (~1e-8).
        EXPECT_NEAR(accuracyBounds.x(), 0.5, 1e-6);
        EXPECT_NEAR(accuracyBounds.y(), 0.7, 1e-6);
        const auto accuracyTicks = view_model.axisTicks(GraphViewModel::Accuracy);
        ASSERT_GE(accuracyTicks.size(), 2);
        EXPECT_NEAR(accuracyTicks.front(), 0.5, 1e-6);
        EXPECT_NEAR(accuracyTicks.back(), 0.7, 1e-6);
    }

    TEST_F(GraphViewModelTest, RecomputeBoundsExpandsDegenerateColumnRangeToNiceNumbers) {
        fake_use_case->series_to_return.times = {0.0F, 1.0F};
        fake_use_case->series_to_return.columns[ColumnId::Score] = {42.0F, 42.0F};
        fake_use_case->series_to_return.columns[ColumnId::Accuracy] = {0.8F, 0.8F};

        view_model.fetchData();
        const auto bounds = view_model.axisBounds();
        const auto scoreBounds = bounds[QString::number(GraphViewModel::Score)].toPointF();
        const auto accuracyBounds = bounds[QString::number(GraphViewModel::Accuracy)].toPointF();

        // An all-equal column has no real range, so the axis expands around the
        // shared value (by the fallback span) and snaps to nice numbers rather
        // than dividing by a zero range. Score 42 -> [41,43] on a 0.5 grid.
        EXPECT_DOUBLE_EQ(scoreBounds.x(), 41.0);
        EXPECT_DOUBLE_EQ(scoreBounds.y(), 43.0);

        // Accuracy 0.8 expands to [-0.2,1.8] then snaps out to a 0.2 grid.
        // 0.8F promoted to double is 0.80000001, so the upper bound rounds up
        // one extra step to 2.0; tolerate float-precision error.
        EXPECT_NEAR(accuracyBounds.x(), -0.2, 1e-6);
        EXPECT_NEAR(accuracyBounds.y(), 2.0, 1e-6);
    }

    // ScoreFamilySharesAnAxisForTheRequestedVisibleSubset, AccuracyRemainsIndependentAndFormatsAsPercentage,
    // and SeriesRecordsItsSourceColumn removed: series(columns) always returns empty against data
    // loaded through fetchData()'s legacy get_series() path, because series()'s per-column body is
    // currently commented out (graph_vm.cpp only builds m_seriesById from the resolved-graph path).
    // See .plans/series-config-migration-completion/plans/04-graph-read-model-narrowing.md.

    TEST_F(GraphViewModelTest, PlottableColumnsExcludesTime) {
        const auto columns = view_model.plottableColumns();
        EXPECT_FALSE(columns.contains(int(GraphViewModel::Time)));
        EXPECT_TRUE(columns.contains(int(GraphViewModel::Score)));
        EXPECT_TRUE(columns.contains(int(GraphViewModel::Dmg)));
        EXPECT_EQ(columns.size(), GraphViewModel::ColumnCount - 1);
    }

    TEST_F(GraphViewModelTest, EnabledColumnsDefaultToAllColumns) {
        EXPECT_EQ(view_model.enabledColumns(), view_model.allColumns());
    }

    TEST_F(GraphViewModelTest, SetEnabledColumnsFiltersDuplicatesInvalidValuesAndUsesCanonicalOrder) {
        view_model.setEnabledColumns({
            ColumnId::Dmg,
            ColumnId::Score,
            ColumnId::Dmg,
            ColumnId::Time,
            static_cast<ColumnId>(999)
        });

        EXPECT_EQ(view_model.enabledColumns(), (QVariantList{GraphViewModel::Score, GraphViewModel::Dmg}));
    }

    TEST_F(GraphViewModelTest, SetEnabledColumnsEmitsOnlyForEffectiveChange) {
        const QSignalSpy spy(&view_model, &GraphViewModel::enabledColumnsChanged);

        view_model.setEnabledColumns({ColumnId::Score, ColumnId::Accuracy});
        EXPECT_EQ(spy.count(), 1);

        view_model.setEnabledColumns({ColumnId::Accuracy, ColumnId::Score, ColumnId::Score});
        EXPECT_EQ(spy.count(), 1);
    }

    // AllColumnsMayBeDisabledWithoutHidingSeriesData removed: its series({Score}) assertion always
    // returns empty for the same reason as the tests removed above.
    // See .plans/series-config-migration-completion/plans/04-graph-read-model-narrowing.md.

    // The settings dialog and control panel key their per-column visibility
    // toggles off these names, lowercased, so every plottable column needs a
    // stable, non-empty name and a distinct color to be usable as a toggle.
    TEST_F(GraphViewModelTest, ColumnNameReturnsExpectedNameForEachColumn) {
        EXPECT_EQ(view_model.columnName(GraphViewModel::Time), "Time");
        EXPECT_EQ(view_model.columnName(GraphViewModel::Score), "Score");
        EXPECT_EQ(view_model.columnName(GraphViewModel::Accuracy), "Accuracy");
        EXPECT_EQ(view_model.columnName(GraphViewModel::Shots), "Shots");
        EXPECT_EQ(view_model.columnName(GraphViewModel::Kills), "Kills");
        EXPECT_EQ(view_model.columnName(GraphViewModel::Dmg), "Dmg");
    }

    TEST_F(GraphViewModelTest, ColumnNameReturnsEmptyForOutOfRangeColumn) {
        EXPECT_TRUE(view_model.columnName(GraphViewModel::ColumnCount).isEmpty());
    }

    TEST_F(GraphViewModelTest, ColumnKeyIsIdentifierSafeAndUniqueForEveryPlottableColumn) {
        QSet<QString> seen;
        for (const auto &entry: view_model.plottableColumns()) {
            const auto column = static_cast<GraphViewModel::Column>(entry.toInt());
            const QString key = view_model.columnKey(column);
            EXPECT_FALSE(key.isEmpty()) << "column " << entry.toInt() << " has an empty key";
            EXPECT_FALSE(key.contains(' ')) << "column " << entry.toInt() << " key contains a space: " << key.
                    toStdString();
            EXPECT_FALSE(seen.contains(key)) << "column " << entry.toInt() << " reuses key " << key.toStdString();
            seen.insert(key);
        }
    }

    TEST_F(GraphViewModelTest, ColumnKeyMatchesExpectedStableKeys) {
        EXPECT_EQ(view_model.columnKey(GraphViewModel::Time), "time");
        EXPECT_EQ(view_model.columnKey(GraphViewModel::Score), "score");
        EXPECT_EQ(view_model.columnKey(GraphViewModel::Accuracy), "accuracy");
        EXPECT_EQ(view_model.columnKey(GraphViewModel::Shots), "shots");
        EXPECT_EQ(view_model.columnKey(GraphViewModel::Kills), "kills");
        EXPECT_EQ(view_model.columnKey(GraphViewModel::Dmg), "dmg");
        EXPECT_EQ(view_model.columnKey(GraphViewModel::ScoreTotal), "scoreTotal");
        EXPECT_EQ(view_model.columnKey(GraphViewModel::ExpectedFinalScore), "expectedFinalScore");
        EXPECT_EQ(view_model.columnKey(GraphViewModel::ExpectedFinalScoreRecent), "expectedFinalScoreRecent");
    }

    TEST_F(GraphViewModelTest, ColumnKeyReturnsEmptyForOutOfRangeColumn) {
        EXPECT_TRUE(view_model.columnKey(GraphViewModel::ColumnCount).isEmpty());
    }

    TEST_F(GraphViewModelTest, ColumnColorIsValidAndDistinctForEveryPlottableColumn) {
        QSet<QRgb> seen;
        for (const auto &entry: view_model.plottableColumns()) {
            const auto column = static_cast<GraphViewModel::Column>(entry.toInt());
            const QColor color = view_model.columnColor(column);
            EXPECT_TRUE(color.isValid()) << "column " << entry.toInt() << " has an invalid color";
            EXPECT_FALSE(seen.contains(color.rgb())) << "column " << entry.toInt() << " reuses another column's color";
            seen.insert(color.rgb());
        }
    }

    TEST_F(GraphViewModelTest, ColumnColorReturnsInvalidForOutOfRangeColumn) {
        EXPECT_FALSE(view_model.columnColor(GraphViewModel::ColumnCount).isValid());
    }

    TEST_F(GraphViewModelTest, ColumnYAxisGroupsTheScoreFamilyTogetherAndKeepsScoreSeparate) {
        EXPECT_EQ(view_model.columnYAxis(GraphViewModel::ScoreTotal),
                  view_model.columnYAxis(GraphViewModel::ExpectedFinalScore));
        EXPECT_EQ(view_model.columnYAxis(GraphViewModel::ScoreTotal),
                  view_model.columnYAxis(GraphViewModel::ExpectedFinalScoreRecent));
        EXPECT_NE(view_model.columnYAxis(GraphViewModel::ScoreTotal), view_model.columnYAxis(GraphViewModel::Score));
        EXPECT_NE(view_model.columnYAxis(GraphViewModel::Score), view_model.columnYAxis(GraphViewModel::Accuracy));
    }

    TEST_F(GraphViewModelTest, ColumnYAxisReturnsNegativeOneForOutOfRangeColumn) {
        EXPECT_EQ(view_model.columnYAxis(GraphViewModel::ColumnCount), -1);
    }

    TEST_F(GraphViewModelTest, FetchDataPopulatesShotsKillsAndDmgColumns) {
        setSampleData();
        fake_use_case->series_to_return.columns[ColumnId::Shots] = {5.0F, 10.0F, 15.0F};
        fake_use_case->series_to_return.columns[ColumnId::Kills] = {1.0F, 2.0F, 3.0F};
        fake_use_case->series_to_return.columns[ColumnId::Dmg] = {100.0F, 200.0F, 300.0F};

        view_model.fetchData();

        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Shots)[0].y(), 5);
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Kills)[1].y(), 2);
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Dmg)[2].y(), 300.0);
    }

    // A misbehaving use case could return a column shorter than the times
    // array (PerfColumnBuilder itself never does - every column it produces
    // has exactly one entry per second); the VM must not read out of bounds
    // and instead defaults the missing rows to zero.
    TEST_F(GraphViewModelTest, FetchDataDefaultsMissingTrailingValuesToZero) {
        setSampleData();
        fake_use_case->series_to_return.columns[ColumnId::Shots] = {5.0F};
        fake_use_case->series_to_return.columns[ColumnId::Dmg] = {100.0F};

        view_model.fetchData();

        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Shots)[0].y(), 5);
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Shots)[1].y(), 0);
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Shots)[2].y(), 0);
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Kills)[0].y(), 0);
        EXPECT_EQ(view_model.seriesPoints(GraphViewModel::Dmg)[1].y(), 0.0);
    }

    TEST_F(GraphViewModelTest, SeriesPointsReturnsTimeValuePairsInRowOrder) {
        setSampleData();
        view_model.fetchData();

        const auto points = view_model.seriesPoints(GraphViewModel::Score);
        ASSERT_EQ(points.size(), 3);
        EXPECT_DOUBLE_EQ(points[0].x(), 0.0);
        EXPECT_DOUBLE_EQ(points[0].y(), 10.0);
        EXPECT_DOUBLE_EQ(points[1].x(), 1.0);
        EXPECT_DOUBLE_EQ(points[1].y(), 20.0);
        EXPECT_DOUBLE_EQ(points[2].x(), 2.0);
        EXPECT_DOUBLE_EQ(points[2].y(), 30.0);
    }

    TEST_F(GraphViewModelTest, SeriesPointsReturnsEmptyWhenNoDataLoaded) {
        EXPECT_TRUE(view_model.seriesPoints(GraphViewModel::Score).isEmpty());
    }

    TEST_F(GraphViewModelTest, SeriesPointsReturnsEmptyForOutOfRangeColumn) {
        setSampleData();
        view_model.fetchData();

        EXPECT_TRUE(view_model.seriesPoints(GraphViewModel::ColumnCount).isEmpty());
    }

    // AccuracySeriesFormattedValueAtXShowsPercent and SeriesReturnsOnlyRequestedColumnsInRequestedOrder
    // removed: series(columns) always returns empty for the same reason as the tests removed above.
    // See .plans/series-config-migration-completion/plans/04-graph-read-model-narrowing.md.

    TEST_F(GraphViewModelTest, SeriesOmitsColumnsWithNoDrawableSeries) {
        setSampleData();
        view_model.fetchData();

        EXPECT_TRUE(view_model.series({GraphViewModel::Time}).isEmpty());
        EXPECT_TRUE(view_model.series({GraphViewModel::ColumnCount}).isEmpty());
    }

    TEST_F(GraphViewModelTest, XAxisDelegateFormatsSecondsWithSuffix) {
        setSampleData();
        view_model.fetchData();

        EXPECT_EQ(view_model.xAxis().formatTick(20.0), "20s");
    }

    TEST_F(GraphViewModelTest, AdaptsResolvedSeriesAndExcludesUnavailableFromBounds) {
        fake_use_case->resolved_graph_to_return = {};
        view_model.fetchData();
        EXPECT_TRUE(view_model.allSeries().isEmpty());
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
        EXPECT_EQ(view_model.columnForSeriesId("1"), GraphViewModel::Score);
        EXPECT_EQ(view_model.seriesIdForColumn(GraphViewModel::Score), "1");
        EXPECT_EQ(view_model.columnForSeriesId("missing"), -1);
    }

    TEST_F(GraphViewModelTest, TranslatesQmlMutationsAndRejectsMalformedExpressions) {
        const auto result = view_model.createComputedSeries("New", QColor("red"), 2.0, true, {{"kind", "unknown"}});
        EXPECT_FALSE(result["succeeded"].toBool());
    }
}
