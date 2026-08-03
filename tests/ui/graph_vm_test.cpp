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

        void load_perf(const std::string_view filename) override { load_perf_calls.emplace_back(filename); }
        std::vector<float> get_times() override { return times; }
        std::vector<float> get_scores() override { return scores; }
        std::vector<float> get_accuracies() override { return accuracies; }
        std::vector<int> get_shots() override { return {}; }
        std::vector<int> get_kills() override { return {}; }
        std::vector<float> get_dmg() override { return {}; }
    };

    class GraphViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphUseCase> fake_use_case = std::make_shared<FakeGraphUseCase>();
        GraphViewModel view_model{fake_use_case};

        void setSampleData() {
            fake_use_case->times = {0.0F, 1.0F, 2.0F};
            fake_use_case->scores = {10.0F, 20.0F, 30.0F};
            fake_use_case->accuracies = {0.5F, 0.6F, 0.7F};
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
        EXPECT_NEAR(accuracyBounds.x(), 0.49, 1e-9);
        EXPECT_NEAR(accuracyBounds.y(), 0.71, 1e-9);
    }

    TEST_F(GraphViewModelTest, RecomputeBoundsPadsDegenerateColumnRange) {
        fake_use_case->times = {5.0F, 5.0F};
        fake_use_case->scores = {42.0F, 42.0F};
        fake_use_case->accuracies = {0.8F, 0.8F};

        view_model.fetchData("");
        const auto bounds = view_model.axisBounds();
        const auto scoreBounds = bounds[QString::number(GraphViewModel::Score)].toPointF();
        const auto accuracyBounds = bounds[QString::number(GraphViewModel::Accuracy)].toPointF();

        // All-equal columns pad by a fixed +-0.5 instead of dividing by a zero range.
        EXPECT_DOUBLE_EQ(scoreBounds.x(), 41.5);
        EXPECT_DOUBLE_EQ(scoreBounds.y(), 42.5);
        EXPECT_DOUBLE_EQ(accuracyBounds.x(), 0.3);
        EXPECT_DOUBLE_EQ(accuracyBounds.y(), 1.3);
    }

    TEST_F(GraphViewModelTest, PlottableColumnsExcludesTime) {
        const auto columns = view_model.plottableColumns();
        EXPECT_FALSE(columns.contains(int(GraphViewModel::Time)));
        EXPECT_TRUE(columns.contains(int(GraphViewModel::Score)));
        EXPECT_TRUE(columns.contains(int(GraphViewModel::Dmg)));
        EXPECT_EQ(columns.size(), GraphViewModel::ColumnCount - 1);
    }

    TEST_F(GraphViewModelTest, ColumnVisibilityDefaultsToTrueAndCanBeToggled) {
        EXPECT_TRUE(view_model.isColumnVisible(GraphViewModel::Score));

        const QSignalSpy spy(&view_model, &GraphViewModel::columnVisibilityChanged);
        view_model.setColumnVisible(GraphViewModel::Score, false);

        EXPECT_FALSE(view_model.isColumnVisible(GraphViewModel::Score));
        EXPECT_EQ(spy.count(), 1);
        EXPECT_FALSE(view_model.columnVisibility()[QString::number(GraphViewModel::Score)].toBool());
    }
}
