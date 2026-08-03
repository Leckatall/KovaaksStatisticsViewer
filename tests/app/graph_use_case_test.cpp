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
        void generateProfileFromDirectory() const override {}

        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }

        void setCurrentPerf(const std::string &filename) override {
            set_current_perf_filename_calls.push_back(filename);
        }

        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }
    };

    ScenarioDataPoint make_point(const float time, const int shots, const int hits, const float score) {
        ScenarioDataPoint point{};
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

    TEST_F(GraphUseCaseTest, GetTimesReturnsAllPointTimes) {
        fake_session_controller->current_perf.data = {
            make_point(0.0F, 1, 1, 0.0F), make_point(1.0F, 1, 1, 0.0F)
        };

        const auto times = use_case.get_times();

        ASSERT_EQ(times.size(), 2);
        EXPECT_FLOAT_EQ(times[0], 0.0F);
        EXPECT_FLOAT_EQ(times[1], 1.0F);
    }

    TEST_F(GraphUseCaseTest, GetScoresReturnsAllPointScores) {
        fake_session_controller->current_perf.data = {make_point(0.0F, 1, 1, 42.0F)};

        const auto scores = use_case.get_scores();

        ASSERT_EQ(scores.size(), 1);
        EXPECT_FLOAT_EQ(scores[0], 42.0F);
    }

    TEST_F(GraphUseCaseTest, GetAccuraciesComputesHitsOverShots) {
        fake_session_controller->current_perf.data = {make_point(0.0F, 10, 5, 0.0F)};

        const auto accuracies = use_case.get_accuracies();

        ASSERT_EQ(accuracies.size(), 1);
        EXPECT_FLOAT_EQ(accuracies[0], 0.5F);
    }

    TEST_F(GraphUseCaseTest, GetAccuraciesReturnsZeroForZeroShotsWithoutExtraEntry) {
        // Regression test for a fixed bug: a zero-shots point used to append
        // both a 0 AND a hits/shots (0/0) value, desyncing the accuracies
        // vector's length from times/scores.
        fake_session_controller->current_perf.data = {
            make_point(0.0F, 0, 0, 0.0F), make_point(1.0F, 10, 5, 0.0F)
        };

        const auto accuracies = use_case.get_accuracies();

        ASSERT_EQ(accuracies.size(), 2);
        EXPECT_FLOAT_EQ(accuracies[0], 0.0F);
        EXPECT_FLOAT_EQ(accuracies[1], 0.5F);
    }

    TEST_F(GraphUseCaseTest, EmptyPerfProducesEmptyVectors) {
        EXPECT_TRUE(use_case.get_times().empty());
        EXPECT_TRUE(use_case.get_scores().empty());
        EXPECT_TRUE(use_case.get_accuracies().empty());
    }
}
