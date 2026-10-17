//
// GraphUseCase tests using a hand-written fake ISessionController.
//

#include <gtest/gtest.h>

#include "usecases/graph_use_case.h"
#include "fake_graph_configuration_use_case.h"
#include "fake_session_controller.h"

using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    class FakeAverageLineUseCase final : public IAverageLineUseCase {
    public:
        [[nodiscard]] std::optional<std::vector<double> > evaluate(
            const ksv::domain::Run &, const Expression &) const override { ++evaluateCallCount; return result; }

        std::optional<std::vector<double> > result;
        mutable int evaluateCallCount = 0;
    };

    class GraphUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> fake_session_controller = std::make_shared<FakeSessionController>();
        std::shared_ptr<FakeGraphConfigurationUseCase> fake_configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();
        std::shared_ptr<FakeAverageLineUseCase> fake_average = std::make_shared<FakeAverageLineUseCase>();
        GraphUseCase use_case{fake_session_controller, fake_configuration, fake_average};
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

    ksv::domain::Run runWithSeconds(const std::string &hash) {
        ksv::domain::Run run;
        run.run_id = {{.name = "S", .hash = hash}, 1000};
        run.performance.emplace();
        run.performance->samples.emplace_back(1.0F);
        run.performance->samples.emplace_back(2.0F);
        return run;
    }

    TEST_F(GraphUseCaseTest, GetSeriesConfigsReturnsEnabledOnly) {
        fake_configuration->configuration.runSeries.series = {
            SeriesConfig{{1}, {"Enabled", {}, true, 0}, primitive(PrimitiveMetric::Score)},
            SeriesConfig{{2}, {"Disabled", {}, false, 1}, primitive(PrimitiveMetric::Shots)},
        };

        const auto configs = use_case.getSeriesConfigs();

        ASSERT_EQ(configs.size(), 1U);
        EXPECT_EQ(configs.front().id.value, 1U);
    }

    TEST_F(GraphUseCaseTest, GetSeriesValuesFoldsBucketTimesIntoPoints) {
        fake_session_controller->current_run = runWithSeconds("a");
        fake_configuration->configuration.runSeries.series = {SeriesConfig{{1}, {"Score", {}, true, 0}, primitive(PrimitiveMetric::Score)}};
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
        fake_session_controller->current_run = runWithSeconds("a");
        fake_configuration->configuration.runSeries.series = {SeriesConfig{{1}, {"Score", {}, true, 0}, primitive(PrimitiveMetric::Score)}};
        fake_average->result = std::nullopt;

        EXPECT_FALSE(use_case.getSeriesValues(SeriesId{1}).has_value());
    }

    TEST_F(GraphUseCaseTest, GetSeriesValuesReturnsNulloptForUnknownId) {
        fake_session_controller->current_run = runWithSeconds("a");
        fake_configuration->configuration.runSeries.series = {SeriesConfig{{1}, {"Score", {}, true, 0}, primitive(PrimitiveMetric::Score)}};
        fake_average->result = std::vector<double>{7.0, 9.0};

        EXPECT_FALSE(use_case.getSeriesValues(SeriesId{999}).has_value());
    }

    TEST_F(GraphUseCaseTest, GetAxesReturnsConfiguredAxes) {
        fake_configuration->configuration.runSeries.axes = {
            AxisConfig{AxisId{1}, "Accuracy", {}, AxisTransformKind::Percentage},
            AxisConfig{AxisId{2}, "Score Family", {}, AxisTransformKind::Identity},
        };

        const auto axes = use_case.getAxes();

        ASSERT_EQ(axes.size(), 2U);
        EXPECT_EQ(axes[0].name, "Accuracy");
        EXPECT_EQ(axes[1].name, "Score Family");
    }

    TEST_F(GraphUseCaseTest, GetRunDurationReturnsRunMaxTime) {
        fake_session_controller->current_run = runWithSeconds("a");

        EXPECT_DOUBLE_EQ(use_case.getRunDuration(), 2.0);
    }

    TEST_F(GraphUseCaseTest, GetRunDurationRefreshesAfterRunChange) {
        fake_session_controller->current_run = runWithSeconds("a");
        EXPECT_DOUBLE_EQ(use_case.getRunDuration(), 2.0);

        ksv::domain::Run longer;
        longer.run_id = {{.name = "S", .hash = "b"}, 2000};
        longer.performance.emplace();
        longer.performance->samples.emplace_back(1.0F);
        longer.performance->samples.emplace_back(2.0F);
        longer.performance->samples.emplace_back(3.0F);
        fake_session_controller->current_run = longer;

        EXPECT_DOUBLE_EQ(use_case.getRunDuration(), 3.0);
    }

    TEST_F(GraphUseCaseTest, HasCurrentRunIsFalseForDefaultRun) {
        EXPECT_FALSE(use_case.hasCurrentRun());
        EXPECT_FALSE(use_case.currentRunHasPerformance());
    }

    // A default run id formats as the epoch ("(1970-01-01, ...)"); with no run selected the label
    // must carry no placeholder date at all.
    TEST_F(GraphUseCaseTest, RunLabelIsEmptyWhenNoRunIsSelected) {
        EXPECT_EQ(use_case.get_run_label(), "");
    }

    TEST_F(GraphUseCaseTest, RunLabelNamesTheScenarioOfTheSelectedRun) {
        fake_session_controller->current_run = runWithSeconds("a");

        const std::string label = use_case.get_run_label();

        EXPECT_EQ(label.rfind("S (", 0), 0U) << label;
        EXPECT_EQ(label, fake_session_controller->current_run.run_id.toString());
    }

    TEST_F(GraphUseCaseTest, PerfRunHasCurrentRunAndPerformance) {
        fake_session_controller->current_run = runWithSeconds("a");

        EXPECT_TRUE(use_case.hasCurrentRun());
        EXPECT_TRUE(use_case.currentRunHasPerformance());
    }

    TEST_F(GraphUseCaseTest, CsvOnlyRunHasCurrentRunButNoPerformance) {
        fake_session_controller->current_run.run_id = {{.name = "S", .hash = "csv"}, 1000};

        EXPECT_TRUE(use_case.hasCurrentRun());
        EXPECT_FALSE(use_case.currentRunHasPerformance());
    }

    TEST_F(GraphUseCaseTest, CsvOnlyCurrentRunProducesEmptyGraphData) {
        fake_session_controller->current_run.run_id = {{.name = "S", .hash = "csv"}, 1000};
        fake_configuration->configuration.runSeries.series = {SeriesConfig{{1}, {"Score", {}, true, 0}, primitive(PrimitiveMetric::Score)}};
        fake_average->result = std::vector<double>{};

        EXPECT_DOUBLE_EQ(use_case.getRunDuration(), 0.0);
        const auto points = use_case.getSeriesValues(SeriesId{1});
        ASSERT_TRUE(points.has_value());
        EXPECT_TRUE(points->empty());
    }

    TEST_F(GraphUseCaseTest, SeriesConfigCallbacksFollowOnlyCommittedRunSeriesChanges) {
        int notifications = 0;
        use_case.onSeriesConfigChanged([&] { ++notifications; });

        fake_configuration->publish({GraphKey::Run, GraphKey::ScenarioHistory}, false, true);
        EXPECT_EQ(notifications, 0);

        fake_configuration->publish({}, true, false);
        EXPECT_EQ(notifications, 1);
    }

    TEST_F(GraphUseCaseTest, AxisLabelIsReadFromCommittedConfigurationAndFollowsOnlyItsChanges) {
        int notifications = 0;
        use_case.onAxisLabelChanged([&] { ++notifications; });
        EXPECT_FALSE(use_case.getAxisLabelSeriesId());

        fake_configuration->configuration.runAxisLabelSeriesId = SeriesId{4};
        fake_configuration->publish({}, true, false);
        EXPECT_EQ(notifications, 0);

        fake_configuration->publish({}, false, true);
        EXPECT_EQ(notifications, 1);
        EXPECT_EQ(use_case.getAxisLabelSeriesId(), SeriesId{4});
    }

    TEST_F(GraphUseCaseTest, RunGraphStyleIsItsConnectionModeAndMarkersFromCommittedConfiguration) {
        EXPECT_EQ(use_case.presentation(), defaultGraphPresentation(GraphKey::Run));

        auto &preferences = fake_configuration->configuration.preferences(GraphKey::Run);
        preferences.appearance = {ConnectionMode::Straight, true};

        EXPECT_EQ(use_case.presentation().appearance, (LineAppearance{ConnectionMode::Straight, true}));
    }

    TEST_F(GraphUseCaseTest, StyleOnlyChangesAnnounceThemselvesWithoutRefreshingSeriesOrAxisLabel) {
        int presentation = 0;
        int series = 0;
        int label = 0;
        use_case.onPresentationChanged([&] { ++presentation; });
        use_case.onSeriesConfigChanged([&] { ++series; });
        use_case.onAxisLabelChanged([&] { ++label; });

        fake_configuration->configuration.preferences(GraphKey::Run).appearance.connection =
            ConnectionMode::PointsOnly;
        fake_configuration->publishDiff();

        EXPECT_EQ(presentation, 1);
        EXPECT_EQ(series, 0);
        EXPECT_EQ(label, 0);
    }

    TEST_F(GraphUseCaseTest, OtherGraphsStyleAndRunSeriesEditsDoNotRestyleTheRunGraph) {
        int presentation = 0;
        use_case.onPresentationChanged([&] { ++presentation; });

        fake_configuration->configuration.preferences(GraphKey::ScenarioHistory).appearance.connection =
            ConnectionMode::Straight;
        fake_configuration->configuration.runAxisLabelSeriesId = SeriesId{2};
        fake_configuration->publishDiff();

        EXPECT_EQ(presentation, 0);
    }
}
