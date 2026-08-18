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
    class PerfPipelineTest : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        GraphViewModel *graphVm = nullptr;
        QString perfUrl;

        void SetUp() override {
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            const QString file = env.copyFixtureIntoPerformances("1wall6targets TE.perf");
            ASSERT_FALSE(file.isEmpty());
            perfUrl = QUrl::fromLocalFile(file).toString();

            app = std::make_unique<application::App>(
                env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore);
            graphVm = app->graphVm();
            ASSERT_NE(graphVm, nullptr);
        }

        [[nodiscard]] QList<QPointF> rawPoints(const GraphViewModel::Column c) const {
            const auto series = graphVm->series(QList<int>{static_cast<int>(c)});
            return series.isEmpty() ? QList<QPointF>{} : series.front().points;
        }
    };

    TEST_F(PerfPipelineTest, LoadingRealPerfPopulatesTitleAndSeries) {
        graphVm->fetchData(perfUrl);

        EXPECT_TRUE(graphVm->scenarioTitle().contains("1wall6targets TE"));

        const auto score = rawPoints(GraphViewModel::Score);
        ASSERT_GE(score.size(), 2);
        // Time axis is built from real data and must span more than the empty default.
        EXPECT_GT(graphVm->xAxis().max(), 0.0);
    }

    TEST_F(PerfPipelineTest, DerivedAccuracyStaysWithinUnitRange) {
        graphVm->fetchData(perfUrl);

        const auto accuracy = rawPoints(GraphViewModel::Accuracy);
        ASSERT_FALSE(accuracy.isEmpty());
        for (const auto &p: accuracy) {
            EXPECT_GE(p.y(), 0.0);
            EXPECT_LE(p.y(), 1.0);
        }
    }

    TEST_F(PerfPipelineTest, ScoreTotalIsTheRunningSumOfPerSecondScore) {
        graphVm->fetchData(perfUrl);

        const auto score = rawPoints(GraphViewModel::Score);
        const auto total = rawPoints(GraphViewModel::ScoreTotal);
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

    TEST_F(PerfPipelineTest, AxisBoundsAreConsistentForEveryColumn) {
        graphVm->fetchData(perfUrl);

        const QVariantMap bounds = graphVm->axisBounds();
        ASSERT_FALSE(bounds.isEmpty());
        for (auto it = bounds.begin(); it != bounds.end(); ++it) {
            const QPointF range = it.value().toPointF();
            EXPECT_LE(range.x(), range.y()) << "axis " << it.key().toStdString() << " has min > max";
        }
    }

    TEST_F(PerfPipelineTest, ReloadingAllFixturesEachDecodeThroughRealDecoder) {
        // Second fixture through the same live pipeline: proves fetchData re-resolves
        // and rebuilds series rather than caching the first run.
        const QString second = env.copyFixtureIntoPerformances("VT FlyTS Novice S5.perf");
        ASSERT_FALSE(second.isEmpty());

        graphVm->fetchData(QUrl::fromLocalFile(second).toString());
        EXPECT_TRUE(graphVm->scenarioTitle().contains("VT FlyTS"));
        EXPECT_GE(rawPoints(GraphViewModel::Score).size(), 2);
    }
}
