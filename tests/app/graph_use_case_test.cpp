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

        void generateProfileFromDirectory() override {
        }

        [[nodiscard]] bool isBuildInProgress() const override { return false; }

        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }

        void setCurrentPerfToLatest() override {
        }

        void setCurrentPerf(const std::string &filename) override {
            set_current_perf_filename_calls.push_back(filename);
        }

        void setCurrentPerf(const ScenarioRunId &) override {
        }

        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }
    };

    class FakeSeriesConfigStore final : public ISeriesConfigStore {
    public:
        [[nodiscard]] std::vector<SeriesConfig> getAll() const override { return configs; }
        MutationResult createComputed(const CreateComputedSeriesRequest &) override { notify(); return {}; }
        MutationResult updateSeries(const UpdateSeriesRequest &) override { notify(); return {}; }
        MutationResult removeComputed(SeriesId) override { notify(); return {}; }
        MutationResult reorder(SeriesId, uint32_t) override { notify(); return {}; }
        void onChanged(std::function<void()> callback) override { callbacks.push_back(std::move(callback)); }

        [[nodiscard]] std::vector<AxisConfig> getAllAxes() const override { return axes; }
        MutationResult createAxis(const CreateAxisRequest &) override { notify(); return {}; }
        MutationResult deleteAxis(AxisId) override { notify(); return {}; }

        void beginDraft() override {}
        MutationResult commitDraft() override { return {}; }
        void discardDraft() override {}
        [[nodiscard]] bool hasPendingChanges() const override { return false; }

        std::vector<SeriesConfig> configs;
        std::vector<AxisConfig> axes;
        std::vector<std::function<void()> > callbacks;

    private:
        void notify() { for (const auto &callback: callbacks) callback(); }
    };

    class FakeAverageLineUseCase final : public IAverageLineUseCase {
    public:
        [[nodiscard]] std::optional<std::vector<double> > evaluate(
            const ScenarioPerf &, const Expression &) const override { ++evaluateCallCount; return result; }

        std::optional<std::vector<double> > result;
        mutable int evaluateCallCount = 0;
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
        std::shared_ptr<FakeSeriesConfigStore> fake_store = std::make_shared<FakeSeriesConfigStore>();
        std::shared_ptr<FakeAverageLineUseCase> fake_average = std::make_shared<FakeAverageLineUseCase>();
        GraphUseCase use_case{fake_session_controller, fake_store, fake_average};
    };

    TEST_F(GraphUseCaseTest, LoadPerfDelegatesToSessionController) {
        use_case.load_perf("some/file.perf");

        ASSERT_EQ(fake_session_controller->set_current_perf_filename_calls.size(), 1);
        EXPECT_EQ(fake_session_controller->set_current_perf_filename_calls[0], "some/file.perf");
    }

    TEST_F(GraphUseCaseTest, LoadPerfPassesOnlyTheGivenViewLengthNotTheWholeUnderlyingBuffer) {
        const std::string buffer = "some/file.perfJUNK-PAST-THE-END";
        const std::string_view truncated(buffer.data(), std::string("some/file.perf").size());

        use_case.load_perf(truncated);

        // Correct contract: only the view's own length reaches the session
        // controller. filename.data() has no length of its own, so passing it to
        // the setCurrentPerf(const std::string&) overload reads until the next
        // NUL - the buffer's real end, not the view's - for a non-terminated
        // substring like this one.
        ASSERT_EQ(fake_session_controller->set_current_perf_filename_calls.size(), 1);
        EXPECT_EQ(fake_session_controller->set_current_perf_filename_calls[0], "some/file.perf");
    }

    TEST_F(GraphUseCaseTest, GetSeriesReturnsAllPointTimes) {
        fake_session_controller->current_perf.data = {
            make_point(1.0F, 1, 1, 0.0F), make_point(2.0F, 1, 1, 0.0F)
        };

        const auto series = use_case.get_series();

        ASSERT_EQ(series.times.size(), 2);
        EXPECT_FLOAT_EQ(series.times[0], 1.0F);
        EXPECT_FLOAT_EQ(series.times[1], 2.0F);
    }

    TEST_F(GraphUseCaseTest, GetSeriesReturnsPointScores) {
        fake_session_controller->current_perf.data = {make_point(1.0F, 1, 1, 42.0F)};

        const auto series = use_case.get_series();

        ASSERT_EQ(series.columns.at(ColumnId::Score).size(), 1);
        EXPECT_FLOAT_EQ(series.columns.at(ColumnId::Score)[0], 42.0F);
    }

    TEST_F(GraphUseCaseTest, GetSeriesComputesAccuracyAsHitsOverShots) {
        fake_session_controller->current_perf.data = {make_point(1.0F, 10, 5, 0.0F)};

        const auto series = use_case.get_series();

        ASSERT_EQ(series.columns.at(ColumnId::Accuracy).size(), 1);
        EXPECT_FLOAT_EQ(series.columns.at(ColumnId::Accuracy)[0], 0.5F);
    }

    TEST_F(GraphUseCaseTest, GetSeriesReturnsZeroAccuracyForZeroShots) {
        fake_session_controller->current_perf.data = {
            make_point(1.0F, 0, 0, 0.0F), make_point(2.0F, 10, 5, 0.0F)
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

    TEST_F(GraphUseCaseTest, ResolvesBaseAndComputedConfigsAsPeers) {
        const auto graph = use_case.get_resolved_graph();
        EXPECT_EQ(graph.series.size(), graph.times.size());
    }

    TEST_F(GraphUseCaseTest, ResolvedGraphIncludesAllConfiguredAxes) {
        fake_store->axes = {
            AxisConfig{AxisId{1}, "Accuracy", {}, AxisTransformKind::Percentage},
            AxisConfig{AxisId{2}, "Score Family", {}, AxisTransformKind::Identity}
        };

        const auto resolved = use_case.get_resolved_graph();

        ASSERT_EQ(resolved.axes.size(), 2U);
        EXPECT_EQ(resolved.axes[0].name, "Accuracy");
        EXPECT_EQ(resolved.axes[1].name, "Score Family");
    }

    TEST_F(GraphUseCaseTest, AverageUnavailableRemainsConfiguredWithoutPoints) {
        const auto graph = use_case.get_resolved_graph();
        EXPECT_TRUE(graph.series.empty() || !graph.series.front().values.has_value());
    }

    TEST_F(GraphUseCaseTest, ResolvedGraphNeverIncludesADisabledSeriesAndNeverEvaluatesItsExpression) {
        fake_store->configs = {
            SeriesConfig{{1}, {"Enabled", {}, true, 0}, primitive(PrimitiveMetric::Score)},
            SeriesConfig{{2}, {"Disabled", {}, false, 1}, primitive(PrimitiveMetric::Shots)},
        };

        const auto resolved = use_case.get_resolved_graph();

        ASSERT_EQ(resolved.series.size(), 1U);
        EXPECT_EQ(resolved.series.front().config.id.value, 1U);
        EXPECT_EQ(fake_average->evaluateCallCount, 1);
    }
}
