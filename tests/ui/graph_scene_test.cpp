#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QStringList>
#include <QVariantList>

#include "components/graph_canvas.h"
#include "components/graph_scene.h"
#include "components/graph_scene_painter.h"
#include "presentation/graphs/common/axes/value_axis.h"
#include "presentation/graphs/common/axes/value_transform.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"

namespace ksv::ui {
    static_assert(std::is_copy_constructible_v<GraphScene>);
    static_assert(std::is_copy_assignable_v<GraphScene>);

    namespace {
        using presentation::SeriesModel;
        using presentation::ValueAxis;
        using presentation::ValueTransform;

        // series() answers in the order lines were added, whatever order the columns are asked for.
        class SceneVm : public presentation::GraphViewModelBase {
        public:
            SceneVm() { m_xAxis.setRange(0.0, 10.0); }

            SeriesModel &addSeries(const int column, const QString &name) {
                auto *series = new SeriesModel(this);
                series->setColumn(column);
                series->setId(QStringLiteral("id-%1").arg(column));
                series->setName(name);
                m_series.append(series);
                return *series;
            }

            void setYAxisColumn(const int column) { m_yAxisColumn = column; }
            [[nodiscard]] ValueAxis &sharedXAxis() { return m_xAxis; }

            [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override {
                QList<SeriesModel *> result;
                for (auto *series: m_series) {
                    if (columns.contains(series->column())) result.append(series);
                }
                return result;
            }

            [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
            [[nodiscard]] int yAxisColumn() const override { return m_yAxisColumn; }

        private:
            ValueAxis m_xAxis;
            QList<SeriesModel *> m_series;
            int m_yAxisColumn = 0;
        };

        ValueTransform suffixed() {
            ValueTransform transform;
            transform.formatter = [](const qreal value) {
                return QString::number(value, 'f', 2) + QStringLiteral(" pts");
            };
            return transform;
        }

        void expectResolved(const GraphSceneAxis &actual, const presentation::Axis &expected) {
            EXPECT_EQ(actual.min, expected.min());
            EXPECT_EQ(actual.max, expected.max());
            ASSERT_FALSE(expected.ticks().isEmpty());
            ASSERT_EQ(actual.ticks, expected.ticks());
            ASSERT_EQ(actual.labels.size(), actual.ticks.size());
            for (qsizetype i = 0; i < actual.ticks.size(); ++i) {
                EXPECT_EQ(actual.labels.at(i), expected.formatTick(actual.ticks.at(i)));
            }
        }

        QList<int> toColumns(const QVariantList &columns) {
            QList<int> result;
            for (const auto &column: columns) result.append(column.toInt());
            return result;
        }

        GraphSceneAxis resolvedAxis(const qreal min, const qreal max) {
            return {min, max, {min, max}, {QString::number(min), QString::number(max)}};
        }

        // One straight horizontal line across the middle of a 0 to 10 by 0 to 100 plot.
        GraphScene handBuiltScene(const QColor &seriesColour) {
            GraphSceneSeries series;
            series.column = 1;
            series.color = seriesColour;
            series.lineWidth = 3.0;
            series.observations = {QPointF(0.0, 50.0), QPointF(10.0, 50.0)};
            series.xAxis = resolvedAxis(0.0, 10.0);
            series.yAxis = resolvedAxis(0.0, 100.0);

            GraphScene scene;
            scene.series = {series};
            scene.xAxis = series.xAxis;
            scene.labelledYAxis = series.yAxis;
            scene.labelledColumn = series.column;
            scene.connection = SeriesPainter::Connection::Straight;
            return scene;
        }
    }

    TEST(GraphSceneStructure, SceneIsAPlainValue) {
        const GraphScene scene;
        const GraphScene fromCanvas = GraphCanvas{}.scene();

        QImage image(30, 30, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        const QRectF bounds(0.0, 0.0, 30.0, 30.0);

        paintGraphScene(painter, scene, bounds);
        paintGraphScene(painter, fromCanvas, bounds);
        static_cast<void>(graphPlotRect(bounds, std::nullopt, std::nullopt));
    }

    TEST(GraphSceneBuild, SeriesFollowTheLineListWithResolvedAppearance) {
        constexpr int kNeutral = 1;
        constexpr int kHidden = 2;
        constexpr int kColoured = 3;
        const QColor foreground(Qt::yellow);
        const QColor ownColour(Qt::blue);

        SceneVm vm;
        SeriesModel &neutral = vm.addSeries(kNeutral, QStringLiteral("Neutral"));
        neutral.lineWidth = 2.0;
        vm.addSeries(kHidden, QStringLiteral("Hidden"));
        SeriesModel &coloured = vm.addSeries(kColoured, QStringLiteral("Coloured"));
        coloured.setColor(ownColour);
        coloured.lineWidth = 5.0;

        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({kColoured, kNeutral});
        canvas.setForegroundColor(foreground);

        const GraphScene scene = canvas.scene();

        ASSERT_EQ(scene.series.size(), 2);
        const GraphSceneSeries &first = scene.series.at(0);
        EXPECT_EQ(first.column, kNeutral);
        EXPECT_EQ(first.id, neutral.id());
        EXPECT_EQ(first.name, neutral.name());
        EXPECT_EQ(first.lineWidth, neutral.lineWidth);
        EXPECT_EQ(first.color, foreground);
        const GraphSceneSeries &second = scene.series.at(1);
        EXPECT_EQ(second.column, kColoured);
        EXPECT_EQ(second.id, coloured.id());
        EXPECT_EQ(second.name, coloured.name());
        EXPECT_EQ(second.lineWidth, coloured.lineWidth);
        EXPECT_EQ(second.color, ownColour);
    }

    TEST(GraphSceneBuild, KeepsObservationsAndCarriesApart) {
        SceneVm vm;
        SeriesModel &series = vm.addSeries(1, QStringLiteral("Carried"));
        series.transform.scale = 2.0;
        series.transform.offset = 1.0;
        series.points = {QPointF(0.0, 1.0), QPointF(1.0, 2.0), QPointF(2.0, 3.0)};
        series.leadingCarry = QPointF(-1.0, 5.0);
        series.trailingCarry = QPointF(3.0, 7.0);

        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({1});

        const GraphScene scene = canvas.scene();

        ASSERT_EQ(scene.series.size(), 1);
        const GraphSceneSeries &built = scene.series.at(0);
        const QList<QPointF> displayed{QPointF(0.0, 3.0), QPointF(1.0, 5.0), QPointF(2.0, 7.0)};
        EXPECT_EQ(built.observations, displayed);
        ASSERT_TRUE(built.leadingCarry.has_value());
        ASSERT_TRUE(built.trailingCarry.has_value());
        EXPECT_EQ(*built.leadingCarry, QPointF(-1.0, 11.0));
        EXPECT_EQ(*built.trailingCarry, QPointF(3.0, 15.0));
        EXPECT_FALSE(built.observations.contains(*built.leadingCarry));
        EXPECT_FALSE(built.observations.contains(*built.trailingCarry));
    }

    TEST(GraphSceneBuild, ResolvesEveryTickToItsLabel) {
        SceneVm vm;
        vm.sharedXAxis().setTransform(suffixed());
        vm.sharedXAxis().setRange(0.0, 10.0);

        SeriesModel &ownAxes = vm.addSeries(1, QStringLiteral("Own axes"));
        ownAxes.transform = suffixed();
        ownAxes.setData({QPointF(0.0, 10.0), QPointF(5.0, 50.0)});
        auto ownXAxis = std::make_shared<ValueAxis>(ValueAxis::Options{}, suffixed());
        ownXAxis->setRange(100.0, 200.0);
        ownAxes.xAxis = ownXAxis;

        SeriesModel &derived = vm.addSeries(2, QStringLiteral("Derived axes"));
        derived.transform = suffixed();
        derived.points = {QPointF(0.0, 300.0), QPointF(5.0, 700.0)};

        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({1, 2});
        canvas.setYAxisColumn(1);

        const GraphScene scene = canvas.scene();

        ASSERT_TRUE(scene.labelledYAxis.has_value());
        expectResolved(*scene.labelledYAxis, *ownAxes.yAxis);
        expectResolved(scene.xAxis, vm.xAxis());
        ASSERT_EQ(scene.series.size(), 2);
        expectResolved(scene.series.at(0).xAxis, *ownXAxis);
        expectResolved(scene.series.at(0).yAxis, *ownAxes.yAxis);
        expectResolved(scene.series.at(1).xAxis, vm.xAxis());
        expectResolved(scene.series.at(1).yAxis, derived.deriveYAxis());
    }

    // With nothing visible the live canvas still labels the view model's own column, which is
    // reachable only through a second series() lookup that is not part of the visible set.
    TEST(GraphSceneBuild, LabelledAxisStillResolvesForAHiddenColumn) {
        constexpr int kHidden = 4;
        SceneVm vm;
        SeriesModel &series = vm.addSeries(kHidden, QStringLiteral("Hidden"));
        series.transform = suffixed();
        series.setData({QPointF(0.0, 10.0), QPointF(5.0, 50.0)});
        vm.setYAxisColumn(kHidden);

        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({});

        const GraphScene scene = canvas.scene();

        ASSERT_TRUE(scene.labelledYAxis.has_value());
        expectResolved(*scene.labelledYAxis, *series.yAxis);
        EXPECT_EQ(scene.labelledColumn, kHidden);
    }

    TEST(GraphSceneBuild, CarriesTheLabelledGroupAndTitle) {
        constexpr int kFirstShared = 10;
        constexpr int kSecondShared = 11;
        constexpr int kIndependent = 12;
        constexpr uint64_t kSharedAxisId = 1;
        const QString sharedAxisName = QStringLiteral("Shared axis");

        SceneVm vm;
        for (const auto &[column, name]: {std::pair{kFirstShared, QStringLiteral("First shared")},
                                          std::pair{kSecondShared, QStringLiteral("Second shared")}}) {
            SeriesModel &shared = vm.addSeries(column, name);
            shared.points = {QPointF(0.0, 0.0), QPointF(10.0, 100.0)};
            shared.yAxisId = kSharedAxisId;
            shared.yAxisName = sharedAxisName;
        }
        vm.addSeries(kIndependent, QStringLiteral("Independent")).points = {QPointF(0.0, 0.0), QPointF(10.0, 1.0)};

        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({kFirstShared, kSecondShared, kIndependent});
        canvas.setYAxisColumn(kSecondShared);

        const GraphScene scene = canvas.scene();

        const QList<int> expectedColumns = toColumns(canvas.labelledYAxisColumns());
        ASSERT_EQ(expectedColumns, (QList<int>{kFirstShared, kSecondShared}));
        ASSERT_EQ(scene.labelledColumns, expectedColumns);
        EXPECT_EQ(scene.labelledYAxisTitle, canvas.labelledYAxisTitle());
        ASSERT_EQ(scene.series.size(), 3);
        EXPECT_EQ(scene.series.at(0).yAxisId, std::optional<uint64_t>(kSharedAxisId));
        EXPECT_EQ(scene.series.at(0).yAxisName, sharedAxisName);
        EXPECT_EQ(scene.series.at(1).yAxisId, std::optional<uint64_t>(kSharedAxisId));
        EXPECT_EQ(scene.series.at(1).yAxisName, sharedAxisName);
        EXPECT_FALSE(scene.series.at(2).yAxisId.has_value());
        EXPECT_TRUE(scene.series.at(2).yAxisName.isEmpty());
    }

    TEST(GraphSceneBuild, CarriesStyleAndReferences) {
        const QColor background(Qt::darkGreen);
        const presentation::ReferenceLine lower{20.0, QColor(Qt::red)};
        const presentation::ReferenceLine upper{40.0, QColor(Qt::cyan)};

        SceneVm vm;
        SeriesModel &series = vm.addSeries(1, QStringLiteral("Referenced"));
        series.setData({QPointF(0.0, 10.0), QPointF(5.0, 50.0)});
        series.referenceLines = {lower, upper};

        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({1});
        canvas.setConnectionMode(GraphCanvas::ConnectionMode::Straight);
        canvas.setPointMarkers(true);
        canvas.setBackgroundColor(background);

        const GraphScene scene = canvas.scene();

        ASSERT_EQ(scene.series.size(), 1);
        const QList<GraphSceneReference> &references = scene.series.at(0).references;
        ASSERT_EQ(references.size(), 2);
        EXPECT_EQ(references.at(0).value, lower.value);
        EXPECT_EQ(references.at(0).color, lower.color);
        EXPECT_EQ(references.at(1).value, upper.value);
        EXPECT_EQ(references.at(1).color, upper.color);
        EXPECT_EQ(scene.connection, SeriesPainter::Connection::Straight);
        EXPECT_TRUE(scene.pointMarkers);
        EXPECT_EQ(scene.background, background);
    }

    TEST(GraphSceneBuild, SelectedBandFollowsMarkedX) {
        constexpr qreal kMarkedX = 5.0;
        const QColor markerColour(Qt::magenta);

        SceneVm vm;
        vm.addSeries(1, QStringLiteral("Marked")).setData({QPointF(0.0, 10.0), QPointF(10.0, 50.0)});

        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({1});
        canvas.setMarkerColor(markerColour);
        canvas.setMarkedX(kMarkedX);

        const GraphScene marked = canvas.scene();

        ASSERT_TRUE(marked.selectedBand.has_value());
        EXPECT_EQ(marked.selectedBand->x, kMarkedX);
        EXPECT_EQ(marked.selectedBand->color, markerColour);

        canvas.setMarkedX(std::numeric_limits<qreal>::quiet_NaN());

        EXPECT_FALSE(canvas.scene().selectedBand.has_value());
    }

    TEST(GraphSceneBuild, SurvivesTheObjectsThatBuiltIt) {
        const QString name = QStringLiteral("Outlived");
        const QList<QPointF> points{QPointF(0.0, 10.0), QPointF(5.0, 50.0)};

        auto vm = std::make_unique<SceneVm>();
        SeriesModel &series = vm->addSeries(1, name);
        series.transform = suffixed();
        series.setData(points);
        auto canvas = std::make_unique<GraphCanvas>();
        canvas->setGraphVm(vm.get());
        canvas->setVisibleColumns({1});

        const GraphScene scene = canvas->scene();

        ASSERT_EQ(scene.series.size(), 1);
        QStringList labels;
        for (const qreal tick: series.yAxis->ticks()) labels.append(series.yAxis->formatTick(tick));
        ASSERT_FALSE(labels.isEmpty());

        canvas.reset();
        vm.reset();

        EXPECT_EQ(scene.series.at(0).name, name);
        EXPECT_EQ(scene.series.at(0).observations, points);
        EXPECT_EQ(scene.series.at(0).yAxis.labels, labels);
    }

    TEST(GraphScenePaint, PaintsAHandBuiltSceneWithoutAViewModel) {
        const QColor seriesColour(Qt::red);
        const GraphScene scene = handBuiltScene(seriesColour);
        QImage image(240, 160, QImage::Format_RGB32);
        image.fill(Qt::black);
        const QRectF bounds(image.rect());

        {
            QPainter painter(&image);
            paintGraphScene(painter, scene, bounds);
        }

        const QRectF plot = graphPlotRect(bounds, scene.labelledYAxis, scene.xAxis);
        const int lineRow = int(plot.center().y());
        int linePixels = 0;
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, lineRow) == seriesColour) ++linePixels;
        }
        ASSERT_GT(linePixels, 0);
        EXPECT_GE(linePixels, int(plot.width()) - 2);
    }

    TEST(GraphScenePaint, StaysInsideOffsetBounds) {
        GraphScene scene = handBuiltScene(QColor(Qt::red));
        scene.selectedBand = GraphSceneBand{5.0, QColor(Qt::magenta)};
        QImage image(400, 300, QImage::Format_RGB32);
        image.fill(Qt::black);
        const QImage untouched = image.copy();
        const QRect bounds(60, 40, 200, 150);

        {
            QPainter painter(&image);
            paintGraphScene(painter, scene, QRectF(bounds));
        }

        int changedInside = 0;
        int changedOutside = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixel(x, y) == untouched.pixel(x, y)) continue;
                ++(bounds.contains(x, y) ? changedInside : changedOutside);
            }
        }
        ASSERT_GT(changedInside, 0);
        EXPECT_EQ(changedOutside, 0);
    }

    TEST(GraphScenePaint, PlotRectMatchesTheCanvasPlotArea) {
        constexpr int kWidth = 800;
        constexpr int kHeight = 600;
        SceneVm vm;
        SeriesModel &series = vm.addSeries(1, QStringLiteral("Wide labels"));
        series.transform.formatter = [](const qreal value) {
            return QString::number(value, 'f', 2) + QStringLiteral(" points per second");
        };
        series.setData({QPointF(0.0, 10.0), QPointF(5.0, 50.0)});

        GraphCanvas canvas;
        canvas.setWidth(kWidth);
        canvas.setHeight(kHeight);
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({1});

        const GraphScene scene = canvas.scene();
        const QRectF plot = graphPlotRect(QRectF(0, 0, kWidth, kHeight), scene.labelledYAxis, scene.xAxis);

        EXPECT_EQ(plot, canvas.property("plotArea").toRectF());
    }

    TEST(GraphScenePaint, ACanvasPaintEqualsItsScenePaint) {
        constexpr int kWidth = 400;
        constexpr int kHeight = 300;
        SceneVm vm;
        SeriesModel &carried = vm.addSeries(1, QStringLiteral("Carried"));
        carried.setColor(Qt::cyan);
        carried.leadingCarry = QPointF(0.0, 20.0);
        carried.trailingCarry = QPointF(10.0, 40.0);
        carried.setData({QPointF(2.0, 30.0), QPointF(5.0, 80.0), QPointF(8.0, 60.0)});
        carried.referenceLines = {{50.0, QColor(Qt::red)}};
        vm.addSeries(2, QStringLiteral("Plain")).setData({QPointF(0.0, 1.0), QPointF(10.0, 2.0)});

        GraphCanvas canvas;
        canvas.setWidth(kWidth);
        canvas.setHeight(kHeight);
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({1, 2});
        canvas.setMarkedX(4.0);

        QImage fromCanvas(kWidth, kHeight, QImage::Format_ARGB32_Premultiplied);
        fromCanvas.fill(Qt::transparent);
        QImage fromScene = fromCanvas.copy();

        {
            QPainter painter(&fromCanvas);
            canvas.paint(&painter);
        }
        {
            QPainter painter(&fromScene);
            paintGraphScene(painter, canvas.scene(), QRectF(0, 0, kWidth, kHeight));
        }

        EXPECT_TRUE(fromCanvas == fromScene);
    }
}
