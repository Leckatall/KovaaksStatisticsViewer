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
        std::vector<float> times;
        std::vector<float> scores;
        std::vector<float> accuracies;
        std::vector<int> shots;
        std::vector<int> kills;
        std::vector<float> dmg;
        std::string run_label;

        void load_perf(const std::string_view filename) override { load_perf_calls.emplace_back(filename); }
        std::vector<float> get_times() override { return times; }
        std::vector<float> get_scores() override { return scores; }
        std::vector<float> get_accuracies() override { return accuracies; }
        std::vector<int> get_shots() override { return shots; }
        std::vector<int> get_kills() override { return kills; }
        std::vector<float> get_dmg() override { return dmg; }
        std::string get_run_label() override { return run_label; }
    };

    class GraphViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphUseCase> fake_use_case = std::make_shared<FakeGraphUseCase>();
        GraphViewModel view_model{fake_use_case};

        void setSampleData() {
            fake_use_case->times = {0.0F, 1.0F, 2.0F};
            fake_use_case->scores = {10.0F, 20.0F, 30.0F};
            fake_use_case->accuracies = {0.5F, 0.6F, 0.7F};
            // Resampling recovers each row's hits as accuracy*shots to merge
            // Accuracy correctly across buckets, so shots must be nonzero
            // wherever a non-zero accuracy is expected to survive resampling.
            fake_use_case->shots = {10, 10, 10};
        }
    };

    TEST_F(GraphViewModelTest, StartsEmptyWithDefaultBounds) {
        EXPECT_EQ(view_model.rowCount(), 0);
        EXPECT_EQ(view_model.columnCount(), GraphViewModel::ColumnCount);
        const auto bounds = view_model.axisBounds();
        EXPECT_DOUBLE_EQ(bounds[QString::number(GraphViewModel::Score)].toPointF().y(), 1.0);
        EXPECT_DOUBLE_EQ(bounds[QString::number(GraphViewModel::Accuracy)].toPointF().x(), 0.0);
        EXPECT_DOUBLE_EQ(bounds[QString::number(GraphViewModel::Accuracy)].toPointF().y(), 1.0);
    }

    TEST_F(GraphViewModelTest, FetchDataWithEmptyIdSkipsLoadPerfButRefreshesSeries) {
        setSampleData();

        view_model.fetchData("");

        EXPECT_TRUE(fake_use_case->load_perf_calls.empty());
        EXPECT_EQ(view_model.rowCount(), 3);
    }

    TEST_F(GraphViewModelTest, FetchDataWithScenarioIdCallsLoadPerfWithLocalPath) {
        setSampleData();

        view_model.fetchData(QUrl::fromLocalFile("C:/perfs/run.perf").toString());

        ASSERT_EQ(fake_use_case->load_perf_calls.size(), 1);
        EXPECT_EQ(fake_use_case->load_perf_calls[0], "C:/perfs/run.perf");
    }

    TEST_F(GraphViewModelTest, FetchDataPopulatesAllThreeColumns) {
        setSampleData();

        view_model.fetchData("");

        EXPECT_EQ(view_model.data(view_model.index(0, GraphViewModel::Time)).toDouble(), 0.0);
        EXPECT_EQ(view_model.data(view_model.index(1, GraphViewModel::Score)).toDouble(), 20.0);
        EXPECT_NEAR(view_model.data(view_model.index(2, GraphViewModel::Accuracy)).toDouble(), 0.7, 1e-6);
    }

    TEST_F(GraphViewModelTest, FetchDataUpdatesScenarioTitleFromUseCase) {
        setSampleData();
        fake_use_case->run_label = "Air Angelic (2026-08-07, 14:23:00)";

        const QSignalSpy spy(&view_model, &GraphViewModel::scenarioTitleChanged);
        view_model.fetchData("");

        EXPECT_EQ(view_model.scenarioTitle(), QString("Air Angelic (2026-08-07, 14:23:00)"));
        EXPECT_EQ(spy.count(), 1);
    }

    TEST_F(GraphViewModelTest, FetchDataEmitsBoundsChanged) {
        setSampleData();

        const QSignalSpy spy(&view_model, &GraphViewModel::boundsChanged);
        view_model.fetchData("");

        EXPECT_GT(spy.count(), 0);
    }

    TEST_F(GraphViewModelTest, RecomputeBoundsUsesRealPerColumnRangesWithPadding) {
        // times {0,1,2}, scores {10,20,30}, accuracies {0.5,0.6,0.7}.
        setSampleData();
        view_model.fetchData("");
        const auto bounds = view_model.axisBounds();
        const auto timeBounds = bounds[QString::number(GraphViewModel::Time)].toPointF();
        const auto scoreBounds = bounds[QString::number(GraphViewModel::Score)].toPointF();
        const auto accuracyBounds = bounds[QString::number(GraphViewModel::Accuracy)].toPointF();

        // Time min is pinned to 0.0 (time never goes negative); max padded by ~5%.
        EXPECT_DOUBLE_EQ(timeBounds.x(), 0.0);
        EXPECT_NEAR(timeBounds.y(), 2.1, 1e-9);

        // Score range [10,30] padded by ~5% on both ends.
        EXPECT_NEAR(scoreBounds.x(), 9.0, 1e-9);
        EXPECT_NEAR(scoreBounds.y(), 31.0, 1e-9);

        // Accuracy range [0.5,0.7] gets its own independent axis, padded ~5%.
        // Accuracies are stored as float and promoted to double, so the
        // tolerance must absorb float-precision error (~1e-8), not just
        // double rounding.
        EXPECT_NEAR(accuracyBounds.x(), 0.49, 1e-6);
        EXPECT_NEAR(accuracyBounds.y(), 0.71, 1e-6);
    }

    TEST_F(GraphViewModelTest, RecomputeBoundsPadsDegenerateColumnRange) {
        // Two distinct, contiguous-from-zero seconds with the same value
        // (rather than one duplicated timestamp, which would land in the
        // same resampled bucket and get summed - correct behavior, but not
        // what this test is after). Starting at 0 avoids resampling
        // zero-filling any leading gap seconds, which would otherwise widen
        // the range with real zeros and defeat the "degenerate range" setup.
        fake_use_case->times = {0.0F, 1.0F};
        fake_use_case->scores = {42.0F, 42.0F};
        fake_use_case->accuracies = {0.8F, 0.8F};
        fake_use_case->shots = {10, 10};

        view_model.fetchData("");
        const auto bounds = view_model.axisBounds();
        const auto scoreBounds = bounds[QString::number(GraphViewModel::Score)].toPointF();
        const auto accuracyBounds = bounds[QString::number(GraphViewModel::Accuracy)].toPointF();

        // All-equal columns pad by a fixed +-0.5 instead of dividing by a zero range.
        EXPECT_DOUBLE_EQ(scoreBounds.x(), 41.5);
        EXPECT_DOUBLE_EQ(scoreBounds.y(), 42.5);
        // Accuracy is stored as float, so 0.8F promoted to double isn't
        // bit-exact; use a tolerance sized to float precision instead of
        // exact equality.
        EXPECT_NEAR(accuracyBounds.x(), 0.3, 1e-6);
        EXPECT_NEAR(accuracyBounds.y(), 1.3, 1e-6);
    }

    TEST_F(GraphViewModelTest, PlottableColumnsExcludesTime) {
        const auto columns = view_model.plottableColumns();
        EXPECT_FALSE(columns.contains(int(GraphViewModel::Time)));
        EXPECT_TRUE(columns.contains(int(GraphViewModel::Score)));
        EXPECT_TRUE(columns.contains(int(GraphViewModel::Dmg)));
        EXPECT_EQ(columns.size(), GraphViewModel::ColumnCount - 1);
    }

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

    TEST_F(GraphViewModelTest, FetchDataPopulatesShotsKillsAndDmgColumns) {
        setSampleData();
        fake_use_case->shots = {5, 10, 15};
        fake_use_case->kills = {1, 2, 3};
        fake_use_case->dmg = {100.0F, 200.0F, 300.0F};

        view_model.fetchData("");

        EXPECT_EQ(view_model.data(view_model.index(0, GraphViewModel::Shots)).toInt(), 5);
        EXPECT_EQ(view_model.data(view_model.index(1, GraphViewModel::Kills)).toInt(), 2);
        EXPECT_EQ(view_model.data(view_model.index(2, GraphViewModel::Dmg)).toDouble(), 300.0);
    }

    // Shots/Kills/Dmg come from separate optional data points than
    // Time/Score/Accuracy, so their arrays can be shorter than the row count;
    // rows past the end of each array fall back to 0 rather than reading OOB.
    TEST_F(GraphViewModelTest, FetchDataDefaultsShotsKillsDmgToZeroWhenArraysShorterThanTimes) {
        setSampleData();
        fake_use_case->shots = {5};
        fake_use_case->kills = {};
        fake_use_case->dmg = {100.0F};

        view_model.fetchData("");

        EXPECT_EQ(view_model.data(view_model.index(0, GraphViewModel::Shots)).toInt(), 5);
        EXPECT_EQ(view_model.data(view_model.index(1, GraphViewModel::Shots)).toInt(), 0);
        EXPECT_EQ(view_model.data(view_model.index(2, GraphViewModel::Shots)).toInt(), 0);
        EXPECT_EQ(view_model.data(view_model.index(0, GraphViewModel::Kills)).toInt(), 0);
        EXPECT_EQ(view_model.data(view_model.index(1, GraphViewModel::Dmg)).toDouble(), 0.0);
    }

    TEST_F(GraphViewModelTest, SeriesPointsReturnsTimeValuePairsInRowOrder) {
        setSampleData();
        view_model.fetchData("");

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
        view_model.fetchData("");

        EXPECT_TRUE(view_model.seriesPoints(GraphViewModel::ColumnCount).isEmpty());
    }

    TEST_F(GraphViewModelTest, DataReturnsInvalidForOutOfRangeRow) {
        setSampleData();
        view_model.fetchData("");

        EXPECT_FALSE(view_model.data(view_model.index(3, GraphViewModel::Score)).isValid());
    }

    // QtGraphs' XYModelMapper (used by LineFromModel.qml) only reacts to
    // rowsInserted/rowsRemoved/dataChanged, not modelReset. A reset desyncs
    // its cached series points from the model, which crashes spline
    // calculation on the next scenario switch. setData() must therefore
    // never reset the model, whether the new dataset is smaller, larger, or
    // the same size as the old one.
    TEST_F(GraphViewModelTest, FetchDataNeverResetsModelWhenSwitchingScenarios) {
        setSampleData();
        view_model.fetchData("");
        ASSERT_EQ(view_model.rowCount(), 3);

        const QSignalSpy resetSpy(&view_model, &GraphViewModel::modelReset);
        const QSignalSpy removeSpy(&view_model, &GraphViewModel::rowsRemoved);
        const QSignalSpy insertSpy(&view_model, &GraphViewModel::rowsInserted);

        // Shrink: fewer rows than currently loaded.
        fake_use_case->times = {0.0F, 1.0F};
        fake_use_case->scores = {10.0F, 20.0F};
        fake_use_case->accuracies = {0.5F, 0.6F};
        view_model.fetchData("");
        EXPECT_EQ(view_model.rowCount(), 2);
        EXPECT_EQ(removeSpy.count(), 1);

        // Grow: more rows than currently loaded.
        fake_use_case->times = {0.0F, 1.0F, 2.0F, 3.0F};
        fake_use_case->scores = {10.0F, 20.0F, 30.0F, 40.0F};
        fake_use_case->accuracies = {0.5F, 0.6F, 0.7F, 0.8F};
        view_model.fetchData("");
        EXPECT_EQ(view_model.rowCount(), 4);
        EXPECT_EQ(insertSpy.count(), 1);

        // Same size: row count unchanged, only values differ.
        fake_use_case->times = {0.0F, 1.0F, 2.0F, 3.0F};
        fake_use_case->scores = {99.0F, 98.0F, 97.0F, 96.0F};
        fake_use_case->accuracies = {0.1F, 0.2F, 0.3F, 0.4F};
        view_model.fetchData("");
        EXPECT_EQ(view_model.rowCount(), 4);
        EXPECT_EQ(view_model.data(view_model.index(0, GraphViewModel::Score)).toDouble(), 99.0);

        EXPECT_EQ(resetSpy.count(), 0);
    }

    // GraphViewModel resamples raw per-tick rows (which arrive as deltas, at
    // irregular sub-second intervals) down to one row per whole second
    // before storing them, so the chart/axis always deal in whole seconds.
    TEST_F(GraphViewModelTest, FetchDataRoundsTimestampsToNearestWholeSecond) {
        fake_use_case->times = {0.3F, 0.6F};
        fake_use_case->scores = {10.0F, 20.0F};
        fake_use_case->accuracies = {0.5F, 0.5F};

        view_model.fetchData("");

        ASSERT_EQ(view_model.rowCount(), 2);
        EXPECT_EQ(view_model.data(view_model.index(0, GraphViewModel::Time)).toDouble(), 0.0);
        EXPECT_EQ(view_model.data(view_model.index(0, GraphViewModel::Score)).toDouble(), 10.0);
        EXPECT_EQ(view_model.data(view_model.index(1, GraphViewModel::Time)).toDouble(), 1.0);
        EXPECT_EQ(view_model.data(view_model.index(1, GraphViewModel::Score)).toDouble(), 20.0);
    }

    TEST_F(GraphViewModelTest, FetchDataFillsSecondsWithNoRawDataAsZero) {
        fake_use_case->times = {0.1F, 3.4F};
        fake_use_case->scores = {10.0F, 20.0F};
        fake_use_case->accuracies = {0.5F, 0.5F};
        fake_use_case->shots = {2, 4};

        view_model.fetchData("");

        // Seconds 0..3: real data at 0 and 3, seconds 1 and 2 have no raw
        // point and must default to zero across every column.
        ASSERT_EQ(view_model.rowCount(), 4);
        for (int second: {1, 2}) {
            EXPECT_EQ(view_model.data(view_model.index(second, GraphViewModel::Score)).toDouble(), 0.0);
            EXPECT_EQ(view_model.data(view_model.index(second, GraphViewModel::Shots)).toDouble(), 0.0);
            EXPECT_EQ(view_model.data(view_model.index(second, GraphViewModel::Accuracy)).toDouble(), 0.0);
        }
        EXPECT_EQ(view_model.data(view_model.index(3, GraphViewModel::Score)).toDouble(), 20.0);
    }

    // The real-world motivating case: near the end of a run, two ticks can
    // land ~0.02s apart (e.g. x.87/x.89) instead of the usual ~1s spacing.
    // Rounded independently, the second tick's small delta would look like
    // an anomalous dip/spike right at the end of the chart; summed into the
    // same bucket as its neighbor, it correctly contributes to that second's
    // total instead.
    TEST_F(GraphViewModelTest, FetchDataSumsAdditiveColumnsWhenPointsRoundIntoTheSameSecond) {
        fake_use_case->times = {58.87F, 58.89F};
        fake_use_case->scores = {1.0F, 2.0F};
        fake_use_case->accuracies = {0.5F, 0.5F};
        fake_use_case->shots = {3, 4};
        fake_use_case->kills = {0, 1};
        fake_use_case->dmg = {10.0F, 20.0F};

        view_model.fetchData("");

        ASSERT_EQ(view_model.rowCount(), 60);
        const auto lastRow = view_model.rowCount() - 1;
        EXPECT_EQ(view_model.data(view_model.index(lastRow, GraphViewModel::Time)).toDouble(), 59.0);
        EXPECT_EQ(view_model.data(view_model.index(lastRow, GraphViewModel::Score)).toDouble(), 3.0);
        EXPECT_EQ(view_model.data(view_model.index(lastRow, GraphViewModel::Shots)).toDouble(), 7.0);
        EXPECT_EQ(view_model.data(view_model.index(lastRow, GraphViewModel::Kills)).toDouble(), 1.0);
        EXPECT_EQ(view_model.data(view_model.index(lastRow, GraphViewModel::Dmg)).toDouble(), 30.0);
    }

    // Accuracy is a ratio, so merging two points that round into the same
    // second must recompute it from summed hits/shots rather than summing or
    // averaging the ratios themselves.
    TEST_F(GraphViewModelTest, FetchDataRecomputesAccuracyFromSummedHitsAndShotsWhenMerging) {
        fake_use_case->times = {10.0F, 10.1F};
        fake_use_case->scores = {0.0F, 0.0F};
        fake_use_case->shots = {10, 90};
        // hits recovered as accuracy*shots: 0.5*10=5, 0.9*90=81 -> merged 86/100=0.86.
        fake_use_case->accuracies = {0.5F, 0.9F};

        view_model.fetchData("");

        // Accuracies are stored as float and promoted to double, so the
        // tolerance must absorb float-precision error, not just double
        // rounding (same reasoning as RecomputeBoundsUsesRealPerColumnRangesWithPadding).
        const double mergedAccuracy = view_model.data(view_model.index(10, GraphViewModel::Accuracy)).toDouble();
        EXPECT_NEAR(mergedAccuracy, 0.86, 1e-6);
        // Guard against regressing to the wrong (naive average) approach.
        EXPECT_NE(mergedAccuracy, 0.7);
    }

    TEST_F(GraphViewModelTest, FetchDataMergedBucketWithZeroShotsHasZeroAccuracy) {
        fake_use_case->times = {20.0F, 20.2F};
        fake_use_case->scores = {0.0F, 0.0F};
        fake_use_case->shots = {0, 0};
        fake_use_case->accuracies = {0.0F, 0.0F};

        view_model.fetchData("");

        EXPECT_EQ(view_model.data(view_model.index(20, GraphViewModel::Accuracy)).toDouble(), 0.0);
    }
}
