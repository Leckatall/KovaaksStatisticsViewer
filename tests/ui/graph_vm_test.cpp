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
        EXPECT_DOUBLE_EQ(view_model.yMax(), 1.0);
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

    TEST_F(GraphViewModelTest, RecomputeBoundsWithDataUsesCurrentFixedRange) {
        // Locks in the current (hardcoded) bounds behavior; the dynamic bounds
        // computation is disabled pending a TODO in GraphViewModel::recomputeBounds.
        setSampleData();
        view_model.fetchData("");

        EXPECT_DOUBLE_EQ(view_model.xMin(), 0.0);
        EXPECT_DOUBLE_EQ(view_model.xMax(), 60.0);
        EXPECT_DOUBLE_EQ(view_model.yMin(), 0.0);
        EXPECT_DOUBLE_EQ(view_model.yMax(), 5.0);
    }
}
