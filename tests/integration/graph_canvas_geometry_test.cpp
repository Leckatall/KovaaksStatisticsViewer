//
// Drives the real GraphCanvas coordinate mapping against a real GraphViewModel
// loaded from a .perf file: valuesAtX / nearestPoint round-trips, plus a pin of
// yAxisFor() deriving a y-axis from the series' own data when the series carries
// no y-axis of its own (graph_canvas.cpp:66).
//

#include <gtest/gtest.h>

#include <QUrl>
#include <QVariantList>
#include <memory>

#include "app/app.h"
#include "components/graph_canvas.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/graph_vm.h"

#include "integration_env.h"

using namespace ksv;
using ui::GraphCanvas;
using presentation::GraphViewModel;

namespace {
    QPointF toPixel(const QPointF &display, const QRectF &rect,
                    const presentation::AxisModel &xAxis, const presentation::AxisModel &yAxis) {
        const qreal xt = xAxis.normalizedPosition(display.x());
        const qreal yt = yAxis.normalizedPosition(display.y());
        return {rect.left() + xt * rect.width(), rect.bottom() - yt * rect.height()};
    }

    class GraphCanvasGeometryTest : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        GraphViewModel *graphVm = nullptr;
        GraphCanvas canvas;

        void SetUp() override {
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            const QString file = env.copyFixtureIntoPerformances("1wall6targets TE.perf");
            ASSERT_FALSE(file.isEmpty());

            app = std::make_unique<application::App>(env.settings, std::make_shared<data::ProtoDecoder>());
            graphVm = app->graphVm();
            graphVm->fetchData(QUrl::fromLocalFile(file).toString());

            canvas.setWidth(800);
            canvas.setHeight(600);
            canvas.setGraphVm(graphVm);
            canvas.setVisibleColumns(QVariantList{static_cast<int>(GraphViewModel::Score)});
        }
    };

    TEST_F(GraphCanvasGeometryTest, ValuesAtXReportsWithinPlotArea) {
        const QRectF rect = canvas.property("plotArea").toRectF();
        ASSERT_GT(rect.width(), 0.0);

        const QVariantMap result = canvas.valuesAtX(rect.center().x());

        ASSERT_TRUE(result.value("valid").toBool());
        const qreal pixelX = result.value("pixelX").toReal();
        EXPECT_GE(pixelX, rect.left());
        EXPECT_LE(pixelX, rect.right());
        EXPECT_FALSE(result.value("series").toList().isEmpty());
    }

    TEST_F(GraphCanvasGeometryTest, NearestPointHitsAKnownScorePixel) {
        const auto series = graphVm->series(QList<int>{static_cast<int>(GraphViewModel::Score)});
        ASSERT_FALSE(series.isEmpty());
        const auto display = series.front().displayPoints();
        ASSERT_FALSE(display.isEmpty());

        const QRectF rect = canvas.property("plotArea").toRectF();
        const auto xAxis = graphVm->xAxis();
        ASSERT_TRUE(series.front().yAxis.has_value());
        const QPointF pixel = toPixel(display.front(), rect, xAxis, *series.front().yAxis);

        const QVariantMap hit = canvas.nearestPoint(pixel.x(), pixel.y());

        ASSERT_TRUE(hit.value("valid").toBool());
        EXPECT_EQ(hit.value("name").toString(), "Score");
    }

    TEST_F(GraphCanvasGeometryTest, NearestPointMissesWhenFarFromEveryPoint) {
        const QVariantMap miss = canvas.nearestPoint(-500.0, -500.0);
        EXPECT_FALSE(miss.value("valid").toBool());
    }

    // Minimal VM whose single series has NO y-axis, forcing yAxisFor() to derive one.
    class NoYAxisVm : public presentation::GraphViewModelBase {
    public:
        [[nodiscard]] QList<presentation::SeriesModel> series(const QList<int> &) const override {
            presentation::SeriesModel s;
            s.name = "Fallback";
            s.points = {QPointF(0.0, 0.0), QPointF(10.0, 100.0)};
            // Deliberately leave xAxis / yAxis unset.
            return {s};
        }
        [[nodiscard]] presentation::AxisModel xAxis() const override {
            return presentation::AxisModel::forRange(0.0, 10.0);
        }
        [[nodiscard]] QVariantList plottableColumns() const override { return {0}; }
        [[nodiscard]] QVariantMap axisBounds() const override { return {}; }
        [[nodiscard]] QList<qreal> axisTicks(int) const override { return {}; }
        [[nodiscard]] QList<QPointF> seriesPoints(int) const override { return {}; }
        [[nodiscard]] QString columnName(int) const override { return "Fallback"; }
        [[nodiscard]] QColor columnColor(int) const override { return {}; }
        [[nodiscard]] QString columnKey(int) const override { return "fallback"; }
        [[nodiscard]] int xColumn() const override { return 0; }
        [[nodiscard]] int yAxisColumn() const override { return 0; }
    };

    TEST_F(GraphCanvasGeometryTest, YAxisDerivedFromSeriesDataWhenUnset) {
        NoYAxisVm vm;
        GraphCanvas fallbackCanvas;
        fallbackCanvas.setWidth(800);
        fallbackCanvas.setHeight(600);
        fallbackCanvas.setGraphVm(&vm);
        fallbackCanvas.setVisibleColumns(QVariantList{0});

        const QRectF rect = fallbackCanvas.property("plotArea").toRectF();
        const auto s = vm.series({0}).front();
        const auto yAxis = s.deriveYAxis();
        const QPointF expected = toPixel(s.displayPoints().back(), rect, vm.xAxis(), yAxis);

        const QVariantMap hit = fallbackCanvas.nearestPoint(expected.x(), expected.y());

        ASSERT_TRUE(hit.value("valid").toBool())
            << "nearestPoint should map y against the y-axis derived from series data; "
               "if this fails, yAxisFor() no longer derives a y-axis for series without one";
        EXPECT_EQ(hit.value("name").toString(), "Fallback");
    }
}
