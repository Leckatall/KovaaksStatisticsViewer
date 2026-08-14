//
// GraphUseCase tests using a hand-written fake ISessionController.
//

#include <gtest/gtest.h>

#include "usecases/graph_use_case.h"

using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSessionController : public ISessionController {
    public:
        ScenarioPerf current_perf;
        std::vector<std::string> set_current_perf_filename_calls;

        std::vector<ScenarioId> getScenarioList() override { return {}; }
        void generateProfileFromDirectory() override {}

        [[nodiscard]] bool isBuildInProgress() const override { return false; }

        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }

        void setCurrentPerfToLatest() override {}

        void setCurrentPerf(const std::string &filename) override {
            set_current_perf_filename_calls.push_back(filename);
        }

        void setCurrentPerf(const ScenarioRunId &) override {}

        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }

        [[nodiscard]] std::vector<ScenarioSummary> getScenarioSummaries() const override { return {}; }

        [[nodiscard]] std::vector<RunPerformance> getRunsForScenario(const ScenarioId &) const override { return {}; }

        [[nodiscard]] std::vector<RunPerformance> getRecentRuns(std::size_t) const override { return {}; }
    };

    ScenarioDataPoint make_point(const float time, const int shots, const int hits, const float score) {
        auto point = ScenarioDataPoint(time);
        point.time = time;
        point.shots = shots;
        point.hits = hits;
        point.score = score;
        return point;
    }

    class GraphUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> fake_session_controller = std::make_shared<FakeSessionController>();
        GraphUseCase use_case{fake_session_controller};
    };

    TEST_F(GraphUseCaseTest, LoadPerfDelegatesToSessionController) {
        use_case.load_perf("some/file.perf");

        ASSERT_EQ(fake_session_controller->set_current_perf_filename_calls.size(), 1);
        EXPECT_EQ(fake_session_controller->set_current_perf_filename_calls[0], "some/file.perf");
    }

    TEST_F(GraphUseCaseTest, GetSeriesReturnsAllPointTimes) {
        fake_session_controller->current_perf.data = {
            make_point(0.0F, 1, 1, 0.0F), make_point(1.0F, 1, 1, 0.0F)
        };

        const auto series = use_case.get_series();

        ASSERT_EQ(series.times.size(), 2);
        EXPECT_FLOAT_EQ(series.times[0], 0.0F);
        EXPECT_FLOAT_EQ(series.times[1], 1.0F);
    }

    TEST_F(GraphUseCaseTest, GetSeriesReturnsPointScores) {
        fake_session_controller->current_perf.data = {make_point(0.0F, 1, 1, 42.0F)};

        const auto series = use_case.get_series();

        ASSERT_EQ(series.columns.at(ColumnId::Score).size(), 1);
        EXPECT_FLOAT_EQ(series.columns.at(ColumnId::Score)[0], 42.0F);
    }

    TEST_F(GraphUseCaseTest, GetSeriesComputesAccuracyAsHitsOverShots) {
        fake_session_controller->current_perf.data = {make_point(0.0F, 10, 5, 0.0F)};

        const auto series = use_case.get_series();

        ASSERT_EQ(series.columns.at(ColumnId::Accuracy).size(), 1);
        EXPECT_FLOAT_EQ(series.columns.at(ColumnId::Accuracy)[0], 0.5F);
    }

    TEST_F(GraphUseCaseTest, GetSeriesReturnsZeroAccuracyForZeroShots) {
        fake_session_controller->current_perf.data = {
            make_point(0.0F, 0, 0, 0.0F), make_point(1.0F, 10, 5, 0.0F)
        };

        const auto series = use_case.get_series();

        ASSERT_EQ(series.columns.at(ColumnId::Accuracy).size(), 2);
        EXPECT_FLOAT_EQ(series.columns.at(ColumnId::Accuracy)[0], 0.0F);
        EXPECT_FLOAT_EQ(series.columns.at(ColumnId::Accuracy)[1], 0.5F);
    }

    TEST_F(GraphUseCaseTest, EmptyPerfProducesEmptySeries) {
        const auto series = use_case.get_series();

        EXPECT_TRUE(series.times.empty());
        EXPECT_TRUE(series.columns.empty());
    }
}
