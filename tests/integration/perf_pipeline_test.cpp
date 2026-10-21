//
// Flagship end-to-end pipeline test: a real .perf file threaded through the
// REAL ProtoDecoder -> FileService -> SessionController -> GraphUseCase
// (PerfColumnBuilder) -> GraphViewModel::fetchData, asserting on the actual VM
// outputs the QML canvas consumes. No fakes anywhere in the chain.
//

#include <gtest/gtest.h>

#include <QUrl>
#include <memory>

#include "app/app.h"
#include "app/contracts/expression_dsl.h"
#include "app/contracts/run_series_editing.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/graph_vm.h"

#include "integration_env.h"

using namespace ksv;
using presentation::GraphViewModel;

namespace {
    // Both tests drive the same wired pipeline and each fetches its own fixture, so
    // the App is built once for the suite rather than per test.
    class PerfPipelineTest : public testing::Test {
    protected:
        static std::unique_ptr<integration::TestEnv> env;
        static std::unique_ptr<application::App> app;
        static GraphViewModel *graphVm;
        static QString perfUrl;

        static void SetUpTestSuite() {
            env = std::make_unique<integration::TestEnv>();
            ASSERT_TRUE(env->valid());
            ASSERT_TRUE(env->makePerformancesDir());
            const QString file = env->copyFixtureIntoPerformances("1wall6targets TE.perf");
            ASSERT_FALSE(file.isEmpty());
            perfUrl = QUrl::fromLocalFile(file).toString();

            app = std::make_unique<application::App>(
                env->settings, std::make_shared<data::ProtoDecoder>(), env->graphConfigurationPersistence,
                env->statsParser, env->benchmarkStore, env->daySource);
            graphVm = app->graphVm();
            ASSERT_NE(graphVm, nullptr);
        }

        // App holds shared_ptrs to services built over env's settings, so it goes first.
        static void TearDownTestSuite() {
            integration::clearSavedGraphConfiguration();
            graphVm = nullptr;
            app.reset();
            env.reset();
        }

        [[nodiscard]] static QList<QPointF> rawPoints(const int column) {
            const auto series = graphVm->series(QList<int>{column});
            return series.isEmpty() ? QList<QPointF>{} : series.front()->points;
        }

        // GraphViewModel::column is a series' SeriesId, not a position. Returns the column of an enabled run
        // line computing `expression`, adding one when the committed lines have none, so the tests do not
        // depend on which lines ship by default. Returns -1 when the line could not be added.
        [[nodiscard]] static int columnFor(const application::Expression &expression, const std::string &name) {
            const auto wanted = application::encodeExpressionDsl(expression);
            auto candidate = *app->graphConfigurationUseCase()->current().configuration;
            for (const auto &series: candidate.runSeries.series) {
                if (series.presentation.enabled && application::encodeExpressionDsl(series.expression) == wanted)
                    return static_cast<int>(series.id.value);
            }
            const auto created = application::createComputedSeries(
                candidate.runSeries, {{name, {{255, 255, 255, 255}, 3.0}, true}, expression});
            if (!created.succeeded() || !app->graphConfigurationUseCase()->save(candidate).accepted()) return -1;
            return static_cast<int>(created.createdId->value);
        }

        [[nodiscard]] static int scoreColumn() {
            return columnFor(application::runningSum(application::primitive(application::PrimitiveMetric::Score)),
                             "Score");
        }

        [[nodiscard]] static int accuracyColumn() {
            using application::PrimitiveMetric;
            return columnFor(application::divide(application::runningSum(application::primitive(PrimitiveMetric::Hits)),
                                                 application::runningSum(application::primitive(PrimitiveMetric::Shots))),
                             "Accuracy");
        }
    };

    std::unique_ptr<integration::TestEnv> PerfPipelineTest::env;
    std::unique_ptr<application::App> PerfPipelineTest::app;
    GraphViewModel *PerfPipelineTest::graphVm = nullptr;
    QString PerfPipelineTest::perfUrl;

    TEST_F(PerfPipelineTest, LoadingRealPerfPopulatesTitleAndSeries) {
        const int score_column = scoreColumn();
        ASSERT_NE(score_column, -1);
        graphVm->fetchData(perfUrl);

        EXPECT_TRUE(graphVm->scenarioTitle().contains("1wall6targets TE"));

        const auto score = rawPoints(score_column);
        ASSERT_GE(score.size(), 2);
        // Time axis is built from real data and must span more than the empty default.
        EXPECT_GT(graphVm->xAxis().max(), 0.0);
    }

    TEST_F(PerfPipelineTest, DerivedAccuracyStaysWithinUnitRange) {
        const int accuracy_column = accuracyColumn();
        ASSERT_NE(accuracy_column, -1);
        graphVm->fetchData(perfUrl);

        const auto accuracy = rawPoints(accuracy_column);
        ASSERT_FALSE(accuracy.isEmpty());
        for (const auto &point: accuracy) {
            EXPECT_GE(point.y(), 0.0);
            EXPECT_LE(point.y(), 1.0);
        }
    }

    TEST_F(PerfPipelineTest, ScoreIsTheRunningSumOfPerSecondScore) {
        const int per_second_column = columnFor(application::primitive(application::PrimitiveMetric::Score),
                                                "Score per second");
        const int score_column = scoreColumn();
        ASSERT_NE(per_second_column, -1);
        ASSERT_NE(score_column, -1);
        graphVm->fetchData(perfUrl);

        const auto perSecond = rawPoints(per_second_column);
        const auto total = rawPoints(score_column);
        ASSERT_GE(total.size(), 2);
        ASSERT_EQ(total.size(), perSecond.size());

        // Score[i] must equal the cumulative sum of per-second score up to i
        // (buckets can be negative in Kovaaks, so it is a running sum, not monotonic).
        qreal running = 0.0;
        for (int i = 0; i < total.size(); ++i) {
            running += perSecond[i].y();
            EXPECT_NEAR(total[i].y(), running, 1e-3) << "running total diverged at index " << i;
        }
    }

    TEST_F(PerfPipelineTest, ReloadingAllFixturesEachDecodeThroughRealDecoder) {
        // Second fixture through the same live pipeline: proves fetchData re-resolves
        // and rebuilds series rather than caching the first run.
        const QString second = env->copyFixtureIntoPerformances("VT FlyTS Novice S5.perf");
        ASSERT_FALSE(second.isEmpty());
        const int score_column = scoreColumn();
        ASSERT_NE(score_column, -1);

        graphVm->fetchData(QUrl::fromLocalFile(second).toString());
        EXPECT_TRUE(graphVm->scenarioTitle().contains("VT FlyTS"));
        EXPECT_GE(rawPoints(score_column).size(), 2);
    }
}
