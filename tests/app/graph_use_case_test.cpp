//
// GraphUseCase tests using a hand-written fake ISessionController.
//

#include <gtest/gtest.h>

#include "usecases/graph_use_case.h"
#include "fake_series_config_store.h"
#include "fake_session_controller.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    class FakeAverageLineUseCase final : public IAverageLineUseCase {
    public:
        [[nodiscard]] std::optional<std::vector<double> > evaluate(
            const ScenarioPerf &, const Expression &) const override { ++evaluateCallCount; return result; }

        std::optional<std::vector<double> > result;
        mutable int evaluateCallCount = 0;
    };

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

    ScenarioPerf perfWithSeconds(const std::string &hash) {
        ScenarioPerf perf;
        perf.run_id = {{.name = "S", .hash = hash}, 1000};
        perf.data.emplace_back(1.0F);
        perf.data.emplace_back(2.0F);
        return perf;
    }

    TEST_F(GraphUseCaseTest, GetSeriesConfigsReturnsEnabledOnly) {
        fake_store->configs = {
            SeriesConfig{{1}, {"Enabled", {}, true, 0}, primitive(PrimitiveMetric::Score)},
            SeriesConfig{{2}, {"Disabled", {}, false, 1}, primitive(PrimitiveMetric::Shots)},
        };

        const auto configs = use_case.getSeriesConfigs();

        ASSERT_EQ(configs.size(), 1U);
        EXPECT_EQ(configs.front().id.value, 1U);
    }

    TEST_F(GraphUseCaseTest, GetSeriesValuesFoldsBucketTimesIntoPoints) {
        fake_session_controller->current_perf = perfWithSeconds("a");
        fake_store->configs = {SeriesConfig{{1}, {"Score", {}, true, 0}, primitive(PrimitiveMetric::Score)}};
        fake_average->result = std::vector<double>{7.0, 9.0};

        const auto points = use_case.getSeriesValues(SeriesId{1});

        ASSERT_TRUE(points.has_value());
        ASSERT_EQ(points->size(), 2U);
        EXPECT_DOUBLE_EQ((*points)[0].first, 1.0);
        EXPECT_DOUBLE_EQ((*points)[0].second, 7.0);
        EXPECT_DOUBLE_EQ((*points)[1].first, 2.0);
        EXPECT_DOUBLE_EQ((*points)[1].second, 9.0);
    }

    TEST_F(GraphUseCaseTest, GetSeriesValuesReturnsNulloptWhenEvaluationFails) {
        fake_session_controller->current_perf = perfWithSeconds("a");
        fake_store->configs = {SeriesConfig{{1}, {"Score", {}, true, 0}, primitive(PrimitiveMetric::Score)}};
        fake_average->result = std::nullopt;

        EXPECT_FALSE(use_case.getSeriesValues(SeriesId{1}).has_value());
    }

    TEST_F(GraphUseCaseTest, GetSeriesValuesReturnsNulloptForUnknownId) {
        fake_session_controller->current_perf = perfWithSeconds("a");
        fake_store->configs = {SeriesConfig{{1}, {"Score", {}, true, 0}, primitive(PrimitiveMetric::Score)}};
        fake_average->result = std::vector<double>{7.0, 9.0};

        EXPECT_FALSE(use_case.getSeriesValues(SeriesId{999}).has_value());
    }

    TEST_F(GraphUseCaseTest, GetAxesReturnsConfiguredAxes) {
        fake_store->axes = {
            AxisConfig{AxisId{1}, "Accuracy", {}, AxisTransformKind::Percentage},
            AxisConfig{AxisId{2}, "Score Family", {}, AxisTransformKind::Identity},
        };

        const auto axes = use_case.getAxes();

        ASSERT_EQ(axes.size(), 2U);
        EXPECT_EQ(axes[0].name, "Accuracy");
        EXPECT_EQ(axes[1].name, "Score Family");
    }

    TEST_F(GraphUseCaseTest, GetRunDurationReturnsRunMaxTime) {
        fake_session_controller->current_perf = perfWithSeconds("a");

        EXPECT_DOUBLE_EQ(use_case.getRunDuration(), 2.0);
    }

    TEST_F(GraphUseCaseTest, GetRunDurationRefreshesAfterRunChange) {
        fake_session_controller->current_perf = perfWithSeconds("a");
        EXPECT_DOUBLE_EQ(use_case.getRunDuration(), 2.0);

        ScenarioPerf longer;
        longer.run_id = {{.name = "S", .hash = "b"}, 2000};
        longer.data.emplace_back(1.0F);
        longer.data.emplace_back(2.0F);
        longer.data.emplace_back(3.0F);
        fake_session_controller->current_perf = longer;

        EXPECT_DOUBLE_EQ(use_case.getRunDuration(), 3.0);
    }
}
