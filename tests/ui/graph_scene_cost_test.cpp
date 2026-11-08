#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QPointF>
#include <QSizeF>
#include <QVariantList>

#include "components/graph_canvas.h"
#include "components/graph_scene.h"
#include "presentation/graphs/common/axes/value_axis.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"

// Measurement, not a performance gate: timings are recorded as GoogleTest XML properties and no
// latency threshold is asserted. Only the scene holding every point is asserted.
//
// A dense paint takes minutes across its repetitions in a Debug build, so paintMs is recorded only
// when KSV_GRAPH_SCENE_COST_PAINT is set.

namespace ksv::ui {
    namespace {
        constexpr int kSceneRepetitions = 20;
        constexpr int kPaintRepetitions = 5;
        constexpr int kWidth = 1600;
        constexpr int kHeight = 900;

        class DenseVm : public presentation::GraphViewModelBase {
        public:
            DenseVm(const int seriesCount, const int pointCount) {
                m_xAxis.setRange(0.0, pointCount - 1.0);
                for (int column = 0; column < seriesCount; ++column) {
                    auto *series = new presentation::SeriesModel(this);
                    series->setColumn(column);
                    series->setId(QStringLiteral("id-%1").arg(column));
                    series->setName(QStringLiteral("Series %1").arg(column));
                    series->setColor(QColor::fromHsv(column * 40, 200, 220));
                    QList<QPointF> points;
                    points.reserve(pointCount);
                    for (int i = 0; i < pointCount; ++i) points.append(QPointF(i, std::sin(i * 0.01) + column));
                    series->points = std::move(points);
                    m_series.append(series);
                }
            }

            [[nodiscard]] QList<presentation::SeriesModel *> series(const QList<int> &columns) const override {
                QList<presentation::SeriesModel *> result;
                for (auto *series: m_series) {
                    if (columns.contains(series->column())) result.append(series);
                }
                return result;
            }

            [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
            [[nodiscard]] int yAxisColumn() const override { return 0; }

        private:
            presentation::ValueAxis m_xAxis;
            QList<presentation::SeriesModel *> m_series;
        };

        double medianMs(const int repetitions, const std::function<void()> &work) {
            std::vector<double> samples;
            samples.reserve(repetitions);
            for (int i = 0; i < repetitions; ++i) {
                QElapsedTimer timer;
                timer.start();
                work();
                samples.push_back(double(timer.nsecsElapsed()) / 1'000'000.0);
            }
            std::ranges::sort(samples);
            return samples[samples.size() / 2];
        }

        void measure(const int seriesCount, const int pointCount) {
            DenseVm vm(seriesCount, pointCount);
            QVariantList visible;
            for (int column = 0; column < seriesCount; ++column) visible.append(column);

            GraphCanvas canvas;
            canvas.setGraphVm(&vm);
            canvas.setVisibleColumns(visible);
            canvas.setSize(QSizeF(kWidth, kHeight));
            canvas.setPointMarkers(true);
            canvas.setConnectionMode(GraphCanvas::ConnectionMode::Smooth);

            const GraphScene scene = canvas.scene();
            ASSERT_EQ(scene.series.size(), seriesCount);
            for (const auto &series: scene.series) EXPECT_EQ(series.observations.size(), pointCount);

            const std::string prefix = std::to_string(seriesCount) + "x" + std::to_string(pointCount) + "_";
            testing::Test::RecordProperty(prefix + "seriesCount", seriesCount);
            testing::Test::RecordProperty(prefix + "pointCount", seriesCount * pointCount);
            testing::Test::RecordProperty(prefix + "sceneBuildMs", std::to_string(medianMs(kSceneRepetitions, [&canvas] {
                static_cast<void>(canvas.scene());
            })));

            if (!qEnvironmentVariableIsSet("KSV_GRAPH_SCENE_COST_PAINT")) return;
            QImage image(kWidth, kHeight, QImage::Format_RGB32);
            testing::Test::RecordProperty(prefix + "paintMs", std::to_string(medianMs(kPaintRepetitions, [&] {
                image.fill(Qt::white);
                QPainter painter(&image);
                canvas.paint(&painter);
            })));
        }
    }

    TEST(GraphSceneCost, DenseGraphsAreMeasured) {
        measure(4, 25'000);
        measure(8, 100'000);
    }
}
