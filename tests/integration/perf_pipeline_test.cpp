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
#include "formats/protobuf/proto_decoder.h"
#include "presentation/graph_vm.h"

#include "integration_env.h"

using namespace ksv;
using presentation::GraphViewModel;

namespace {
    // Built-in SeriesId from defaultSeriesConfigs() — GraphViewModel::column is always a series'
    // SeriesId, not a position, and has no dedicated enum outside the class.
    constexpr int kScoreSeriesId = 1;
    constexpr int kAccuracySeriesId = 2;
    constexpr int kScoreTotalSeriesId = 7;

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
                env->settings, std::make_shared<data::ProtoDecoder>(), env->seriesConfigStore);
            graphVm = app->graphVm();
            ASSERT_NE(graphVm, nullptr);
        }

        // App holds shared_ptrs to services built over env's settings, so it goes first.
        static void TearDownTestSuite() {
            graphVm = nullptr;
            app.reset();
            env.reset();
        }

        [[nodiscard]] static QList<QPointF> rawPoints(const int column) {
            const auto series = graphVm->series(QList<int>{column});
            return series.isEmpty() ? QList<QPointF>{} : series.front()->points;
        }
    };

    std::unique_ptr<integration::TestEnv> PerfPipelineTest::env;
    std::unique_ptr<application::App> PerfPipelineTest::app;
    GraphViewModel *PerfPipelineTest::graphVm = nullptr;
    QString PerfPipelineTest::perfUrl;

    TEST_F(PerfPipelineTest, LoadingRealPerfPopulatesTitleAndSeries) {
        graphVm->fetchData(perfUrl);

        EXPECT_TRUE(graphVm->scenarioTitle().contains("1wall6targets TE"));

        const auto score = rawPoints(kScoreSeriesId);
        ASSERT_GE(score.size(), 2);
        // Time axis is built from real data and must span more than the empty default.
        EXPECT_GT(graphVm->xAxis().max(), 0.0);
    }

    TEST_F(PerfPipelineTest, DerivedAccuracyStaysWithinUnitRange) {
        graphVm->fetchData(perfUrl);

        const auto accuracy = rawPoints(kAccuracySeriesId);
        ASSERT_FALSE(accuracy.isEmpty());
        for (const auto &point: accuracy) {
            EXPECT_GE(point.y(), 0.0);
            EXPECT_LE(point.y(), 1.0);
        }
    }

    TEST_F(PerfPipelineTest, ScoreTotalIsTheRunningSumOfPerSecondScore) {
        graphVm->fetchData(perfUrl);

        const auto score = rawPoints(kScoreSeriesId);
        const auto total = rawPoints(kScoreTotalSeriesId);
        ASSERT_GE(total.size(), 2);
        ASSERT_EQ(total.size(), score.size());

        // ScoreTotal[i] must equal the cumulative sum of per-second Score up to i
        // (buckets can be negative in Kovaaks, so it is a running sum, not monotonic).
        qreal running = 0.0;
        for (int i = 0; i < total.size(); ++i) {
            running += score[i].y();
            EXPECT_NEAR(total[i].y(), running, 1e-3) << "running total diverged at index " << i;
        }
    }

    TEST_F(PerfPipelineTest, ReloadingAllFixturesEachDecodeThroughRealDecoder) {
        // Second fixture through the same live pipeline: proves fetchData re-resolves
        // and rebuilds series rather than caching the first run.
        const QString second = env->copyFixtureIntoPerformances("VT FlyTS Novice S5.perf");
        ASSERT_FALSE(second.isEmpty());

        graphVm->fetchData(QUrl::fromLocalFile(second).toString());
        EXPECT_TRUE(graphVm->scenarioTitle().contains("VT FlyTS"));
        EXPECT_GE(rawPoints(kScoreSeriesId).size(), 2);
    }
}
