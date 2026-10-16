//
// Drives the real GraphCanvas coordinate mapping against a real GraphViewModel
// loaded from a .perf file: valuesAtX / nearestPoint round-trips, plus a pin of
// yAxisFor() deriving a y-axis from the series' own data when the series carries
// no y-axis of its own (graph_canvas.cpp:66).
//

#include <gtest/gtest.h>

#include <QImage>
#include <QPainter>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <memory>

#include "app/app.h"
#include "app/contracts/i_completion_history_use_case.h"
#include "components/graph_canvas.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/completion_history_vm.h"
#include "presentation/graph_vm.h"

#include "integration_env.h"

using namespace ksv;
using ui::GraphCanvas;
using presentation::GraphViewModel;

namespace {
    // Built-in SeriesIds from defaultSeriesConfigs() — GraphViewModel::column is always a series'
    // SeriesId, not a position, and has no dedicated enum outside the class.
    constexpr int kScoreSeriesId = 1;
    constexpr int kAccuracySeriesId = 2;
    // Score Total, Expected Final Score and Expected Final Score (5s) share the "Score Family" axis.
    constexpr int kScoreTotalSeriesId = 7;
    constexpr int kExpectedFinalSeriesId = 8;
    constexpr int kExpectedFinal5sSeriesId = 9;

    void configureCanvas(GraphCanvas &canvas, presentation::GraphViewModelBase *vm, const QVariantList &columns) {
        canvas.setWidth(800);
        canvas.setHeight(600);
        canvas.setGraphVm(vm);
        canvas.setVisibleColumns(columns);
    }

    std::unique_ptr<GraphCanvas> makeCanvas(presentation::GraphViewModelBase &vm, const QVariantList &columns) {
        auto canvas = std::make_unique<GraphCanvas>();
        configureCanvas(*canvas, &vm, columns);
        return canvas;
    }

    QPointF toPixel(const QPointF &display, const QRectF &rect,
                    const presentation::Axis &xAxis, const presentation::Axis &yAxis) {
        const qreal xt = xAxis.normalizedPosition(display.x());
        const qreal yt = yAxis.normalizedPosition(display.y());
        return {rect.left() + xt * rect.width(), rect.bottom() - yt * rect.height()};
    }

    // No test here mutates the view model, so the App and its one decode are built once
    // for the suite. The canvas is NOT shared -- every test reconfigures its visible
    // columns and y-axis column, so each gets its own.
    class GraphCanvasGeometryTest : public testing::Test {
    protected:
        static std::unique_ptr<integration::TestEnv> env;
        static std::unique_ptr<application::App> app;
        static GraphViewModel *graphVm;

        static void SetUpTestSuite() {
            env = std::make_unique<integration::TestEnv>();
            ASSERT_TRUE(env->valid());
            ASSERT_TRUE(env->makePerformancesDir());
            const QString file = env->copyFixtureIntoPerformances("1wall6targets TE.perf");
            ASSERT_FALSE(file.isEmpty());

            app = std::make_unique<application::App>(
                env->settings, std::make_shared<data::ProtoDecoder>(), env->graphConfigurationPersistence,
                env->statsParser, env->benchmarkStore, env->daySource);
            graphVm = app->graphVm();
            graphVm->fetchData(QUrl::fromLocalFile(file).toString());
        }

        // App holds shared_ptrs to services built over env's settings, so it goes first.
        static void TearDownTestSuite() {
            graphVm = nullptr;
            app.reset();
            env.reset();
        }

        GraphCanvas canvas;

        void SetUp() override {
            ASSERT_NE(graphVm, nullptr);
            configureCanvas(canvas, graphVm, QVariantList{kScoreSeriesId});
        }
    };

    std::unique_ptr<integration::TestEnv> GraphCanvasGeometryTest::env;
    std::unique_ptr<application::App> GraphCanvasGeometryTest::app;
    GraphViewModel *GraphCanvasGeometryTest::graphVm = nullptr;

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

    TEST_F(GraphCanvasGeometryTest, ValuesAtXReportsEachSeriesPixelYWithinPlotArea) {
        canvas.setVisibleColumns(QVariantList{kScoreSeriesId, kAccuracySeriesId});
        const QRectF rect = canvas.property("plotArea").toRectF();

        const QVariantMap result = canvas.valuesAtX(rect.center().x());

        ASSERT_TRUE(result.value("valid").toBool());
        const QVariantList series = result.value("series").toList();
        ASSERT_EQ(series.size(), 2);
        for (const QVariant &value: series) {
            const QVariantMap entry = value.toMap();
            ASSERT_TRUE(entry.contains("pixelY"));
            EXPECT_GE(entry.value("pixelY").toReal(), rect.top());
            EXPECT_LE(entry.value("pixelY").toReal(), rect.bottom());
        }
    }

    TEST_F(GraphCanvasGeometryTest, NearestPointHitsAKnownScorePixel) {
        const auto series = graphVm->series(QList<int>{kScoreSeriesId});
        ASSERT_FALSE(series.isEmpty());
        const auto display = series.front()->displayPoints();
        ASSERT_FALSE(display.isEmpty());

        const QRectF rect = canvas.property("plotArea").toRectF();
        const auto &xAxis = graphVm->xAxis();
        // Score isn't in GraphViewModel's hardcoded axis table, so it derives its own axis the same
        // way GraphCanvas does — see yAxisFor() in graph_canvas.cpp.
        const QPointF pixel = toPixel(display.front(), rect, xAxis,
                                      series.front()->yAxis.value_or(series.front()->deriveYAxis()));

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
        NoYAxisVm() { m_xAxis.setRange(0.0, 10.0); }

        [[nodiscard]] QList<presentation::SeriesModel *> series(const QList<int> &) const override {
            auto *s = new presentation::SeriesModel(const_cast<NoYAxisVm *>(this));
            s->setName("Fallback");
            s->setColumn(0);
            s->points = {QPointF(0.0, 0.0), QPointF(10.0, 100.0)};
            // Deliberately leave xAxis / yAxis unset.
            return {s};
        }
        [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
        [[nodiscard]] int yAxisColumn() const override { return 0; }

    private:
        presentation::ValueAxis m_xAxis;
    };

    TEST_F(GraphCanvasGeometryTest, YAxisDerivedFromSeriesDataWhenUnset) {
        NoYAxisVm vm;
        const auto fallbackCanvas = makeCanvas(vm, QVariantList{0});

        const QRectF rect = fallbackCanvas->property("plotArea").toRectF();
        const auto s = vm.series({0}).front();
        const auto yAxis = s->deriveYAxis();
        const QPointF expected = toPixel(s->displayPoints().back(), rect, vm.xAxis(), yAxis);

        const QVariantMap hit = fallbackCanvas->nearestPoint(expected.x(), expected.y());

        ASSERT_TRUE(hit.value("valid").toBool())
            << "nearestPoint should map y against the y-axis derived from series data; "
               "if this fails, yAxisFor() no longer derives a y-axis for series without one";
        EXPECT_EQ(hit.value("name").toString(), "Fallback");
    }

    TEST_F(GraphCanvasGeometryTest, LabelledYAxisColumnDefaultsToVmYAxisColumnWhenUnset) {
        canvas.setVisibleColumns(QVariantList{kScoreSeriesId});
        EXPECT_EQ(canvas.property("labelledYAxisColumn").toInt(), kScoreSeriesId);
    }

    TEST_F(GraphCanvasGeometryTest, LabelledYAxisColumnReturnsRequestedColumnWhenVisible) {
        canvas.setVisibleColumns(QVariantList{kScoreSeriesId,
                                               kAccuracySeriesId});
        canvas.setYAxisColumn(kAccuracySeriesId);
        EXPECT_EQ(canvas.property("labelledYAxisColumn").toInt(), kAccuracySeriesId);
    }

    TEST_F(GraphCanvasGeometryTest, LabelledYAxisColumnFallsBackToFirstVisibleWhenRequestedIsHidden) {
        canvas.setVisibleColumns(QVariantList{kAccuracySeriesId});
        canvas.setYAxisColumn(kScoreSeriesId);
        EXPECT_EQ(canvas.property("labelledYAxisColumn").toInt(), kAccuracySeriesId);
    }

    TEST_F(GraphCanvasGeometryTest, LabelledYAxisColumnFallsBackToVmDefaultWhenNothingVisible) {
        canvas.setVisibleColumns(QVariantList{});
        canvas.setYAxisColumn(kAccuracySeriesId);
        EXPECT_EQ(canvas.property("labelledYAxisColumn").toInt(), graphVm->yAxisColumn());
    }

    // Minimal VM whose y-range is caller-controlled, to compare margins for narrow vs. wide tick text.
    class FixedRangeVm : public presentation::GraphViewModelBase {
    public:
        explicit FixedRangeVm(const qreal yHi) : m_yHi(yHi) { m_xAxis.setRange(0.0, 10.0); }

        [[nodiscard]] QList<presentation::SeriesModel *> series(const QList<int> &) const override {
            auto *s = new presentation::SeriesModel(const_cast<FixedRangeVm *>(this));
            s->setName("Value");
            s->setColumn(0);
            s->points = {QPointF(0.0, 0.0), QPointF(10.0, m_yHi)};
            return {s};
        }
        [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
        [[nodiscard]] int yAxisColumn() const override { return 0; }

    private:
        qreal m_yHi;
        presentation::ValueAxis m_xAxis;
    };

    TEST_F(GraphCanvasGeometryTest, LeftMarginGrowsWithWiderTickLabels) {
        FixedRangeVm narrowVm(1.0);
        const auto narrowCanvas = makeCanvas(narrowVm, QVariantList{0});

        FixedRangeVm wideVm(123456.0);
        const auto wideCanvas = makeCanvas(wideVm, QVariantList{0});

        const qreal narrowLeft = narrowCanvas->property("plotArea").toRectF().left();
        const qreal wideLeft = wideCanvas->property("plotArea").toRectF().left();

        EXPECT_GT(wideLeft, narrowLeft)
            << "left margin should grow to fit wider y-axis tick text instead of staying fixed";
    }

    class VisibleSetAxisVm : public presentation::GraphViewModelBase {
    public:
        VisibleSetAxisVm() { m_xAxis.setRange(0.0, 10.0); }

        [[nodiscard]] QList<presentation::SeriesModel *> series(const QList<int> &columns) const override {
            const bool shared = columns.contains(0) && columns.contains(1);
            presentation::ValueAxis yAxis;
            yAxis.setRange(0.0, shared ? 123456.0 : 1.0);
            QList<presentation::SeriesModel *> result;
            for (const int column: columns) {
                if (column < 0 || column > 1) continue;
                auto *s = new presentation::SeriesModel(const_cast<VisibleSetAxisVm *>(this));
                s->setColumn(column);
                s->setName("Value");
                s->points = {QPointF(0.0, 0.0), QPointF(10.0, shared ? 123456.0 : 1.0)};
                s->yAxis = yAxis;
                result.append(s);
            }
            return result;
        }
        [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
        [[nodiscard]] int yAxisColumn() const override { return 0; }

    private:
        presentation::ValueAxis m_xAxis;
    };

    TEST_F(GraphCanvasGeometryTest, LabelledAxisUsesTheVisibleSeriesSet) {
        VisibleSetAxisVm vm;
        const auto canvas = makeCanvas(vm, QVariantList{0});
        const qreal singleLeft = canvas->property("plotArea").toRectF().left();

        canvas->setVisibleColumns(QVariantList{0, 1});
        const qreal sharedLeft = canvas->property("plotArea").toRectF().left();

        EXPECT_GT(sharedLeft, singleLeft);
    }

    // --- Which lines the labelled Y axis measures. ---

    TEST_F(GraphCanvasGeometryTest, IndependentlyScaledLinesLeaveTheAxisToItsOwnLine) {
        canvas.setVisibleColumns(QVariantList{kScoreSeriesId, kAccuracySeriesId});
        canvas.setYAxisColumn(kScoreSeriesId);

        EXPECT_EQ(canvas.property("labelledYAxisColumns").toList(), QVariantList{kScoreSeriesId});
    }

    TEST_F(GraphCanvasGeometryTest, LinesSharingTheLabelledAxisAreAllMeasuredByIt) {
        canvas.setVisibleColumns(QVariantList{kScoreTotalSeriesId, kExpectedFinalSeriesId, kExpectedFinal5sSeriesId});
        canvas.setYAxisColumn(kScoreTotalSeriesId);

        EXPECT_EQ(canvas.property("labelledYAxisColumns").toList(),
                  (QVariantList{kScoreTotalSeriesId, kExpectedFinalSeriesId, kExpectedFinal5sSeriesId}));
    }

    TEST_F(GraphCanvasGeometryTest, MixedLinesNameOnlyTheSharedGroup) {
        canvas.setVisibleColumns(QVariantList{kScoreSeriesId, kScoreTotalSeriesId, kExpectedFinalSeriesId});
        canvas.setYAxisColumn(kExpectedFinalSeriesId);

        EXPECT_EQ(canvas.property("labelledYAxisColumns").toList(),
                  (QVariantList{kScoreTotalSeriesId, kExpectedFinalSeriesId}));
    }

    // --- Lines stack in the user's line-list order. ---

    // The labelled axis no longer pulls its line to the front: the first series in the list is
    // painted last (on top) whatever order the columns are made visible in or which line owns the
    // axis, and the hover readout lists the same series top-of-list first.
    TEST_F(GraphCanvasGeometryTest, LinesArePaintedBottomOfTheListFirst) {
        canvas.setVisibleColumns(QVariantList{kScoreTotalSeriesId, kAccuracySeriesId, kScoreSeriesId});
        canvas.setYAxisColumn(kScoreTotalSeriesId);

        EXPECT_EQ(canvas.paintOrder(), (QList<int>{kScoreTotalSeriesId, kAccuracySeriesId, kScoreSeriesId}));

        const QRectF rect = canvas.property("plotArea").toRectF();
        const QVariantList hovered = canvas.valuesAtX(rect.center().x()).value("series").toList();
        QStringList names;
        for (const auto &entry: hovered) names.append(entry.toMap().value("name").toString());
        EXPECT_EQ(names, (QStringList{"Score", "Accuracy", "Score Total"}));
    }

    TEST_F(GraphCanvasGeometryTest, PaintOrderReversesTheEnabledLineList) {
        QVariantList visible;
        QList<int> expected;
        for (const auto &id: graphVm->enabledSeriesIds()) {
            const int column = graphVm->columnForSeriesId(id.toString());
            if (graphVm->series({column}).isEmpty()) continue;
            visible.append(column);
            expected.prepend(column);
        }
        ASSERT_GE(expected.size(), 2);
        canvas.setVisibleColumns(visible);

        EXPECT_EQ(canvas.paintOrder(), expected);
        EXPECT_EQ(canvas.paintOrder().back(), kScoreSeriesId) << "Score heads the default list, so it is on top";
    }

    // --- Real data points are marked while sparse. ---

    TEST(GraphCanvasMarkersTest, MarkersFitOnlyWhenPointsAreFarEnoughApart) {
        EXPECT_TRUE(GraphCanvas::markersFit(1, 0.0)) << "a lone point is always marked";
        EXPECT_TRUE(GraphCanvas::markersFit(8, 600.0));
        EXPECT_TRUE(GraphCanvas::markersFit(8, 400.0));
        EXPECT_FALSE(GraphCanvas::markersFit(224, 570.0));
        EXPECT_FALSE(GraphCanvas::markersFit(0, 600.0));
    }

    // A run-history-like VM: `count` runs plotted against run index.
    class RunsVm : public presentation::GraphViewModelBase {
    public:
        explicit RunsVm(const int count) : m_series(new presentation::SeriesModel(this)) {
            m_xAxis.setRange(1.0, count);
            m_series->setColumn(0);
            m_series->setColor(Qt::green);
            QList<QPointF> points;
            for (int i = 1; i <= count; ++i) points.append(QPointF(i, 100.0 + (i % 7)));
            m_series->setData(points);
        }

        [[nodiscard]] QList<presentation::SeriesModel *> series(const QList<int> &columns) const override {
            return columns.contains(0) ? QList<presentation::SeriesModel *>{m_series} : QList<presentation::SeriesModel *>{};
        }
        [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
        [[nodiscard]] int yAxisColumn() const override { return 0; }

    private:
        presentation::ValueAxis m_xAxis;
        presentation::SeriesModel *m_series;
    };

    TEST(GraphCanvasMarkersTest, FewRunsAreMarkedAndManyRunsStayPlainLines) {
        RunsVm few(8);
        RunsVm many(224);
        const auto fewCanvas = makeCanvas(few, QVariantList{0});
        const auto manyCanvas = makeCanvas(many, QVariantList{0});

        EXPECT_FALSE(fewCanvas->property("pointMarkersShown").toBool()) << "markers are opt-in";

        fewCanvas->setProperty("pointMarkers", true);
        manyCanvas->setProperty("pointMarkers", true);
        EXPECT_TRUE(fewCanvas->property("pointMarkersShown").toBool());
        EXPECT_FALSE(manyCanvas->property("pointMarkersShown").toBool());
    }

    class OneRunHistory final : public application::ICompletionHistoryUseCase {
    public:
        application::CompletionHistory get_history() override {
            return {.scenario_name = "Air Angelic",
                    .full_count = 9,
                    .rows = {{.run_index = 7, .score = 125.0, .accuracy = 0.6, .shots = 25, .hits = 15, .misses = 10}}};
        }
        void onCurrentScenarioChanged(std::function<void()>) override {}
        void onRangeChanged(std::function<void()>) override {}
        std::optional<int> selected_run_index() override { return 7; }
        void onSelectedRunChanged(std::function<void()>) override {}
        void select_run(int) override {}
    };

    // The history chart used to refuse fewer than two runs; a lone visible run must be markable and
    // hoverable instead.
    TEST(GraphCanvasMarkersTest, AOneRunHistoryIsMarkedAndInspectable) {
        presentation::CompletionHistoryViewModel history(std::make_shared<OneRunHistory>());
        const auto canvas = makeCanvas(history, QVariantList{presentation::CompletionHistoryViewModel::Score});
        canvas->setProperty("pointMarkers", true);
        canvas->setMarkedX(7.0);

        EXPECT_TRUE(canvas->property("pointMarkersShown").toBool());
        const QRectF plot = canvas->property("plotArea").toRectF();
        const QVariantMap hover = canvas->valuesAtX(plot.center().x());
        ASSERT_TRUE(hover.value("valid").toBool());
        EXPECT_DOUBLE_EQ(hover.value("xRaw").toReal(), 7.0);
        EXPECT_EQ(hover.value("x").toString(), "#7");
        const QVariantList series = hover.value("series").toList();
        ASSERT_EQ(series.size(), 1);
        EXPECT_TRUE(series.front().toMap().value("pixelY").isValid());
        EXPECT_NEAR(hover.value("pixelX").toReal(), plot.center().x(), 1.0) << "the lone run sits mid-axis";

        const QVariantMap nearest = canvas->nearestPoint(hover.value("pixelX").toReal(),
                                                          series.front().toMap().value("pixelY").toReal());
        EXPECT_TRUE(nearest.value("valid").toBool());
    }

    // --- Connection modes, widths, clipping and non-selectable geometry. ---

    constexpr QRgb kSeriesRed = qRgb(255, 0, 0);
    constexpr QRgb kReferenceBlue = qRgb(0, 0, 255);

    class StyledVm : public presentation::GraphViewModelBase {
    public:
        StyledVm(QList<QPointF> points, const qreal xLo, const qreal xHi) : m_series(new presentation::SeriesModel(this)) {
            m_xAxis.setRange(xLo, xHi);
            m_series->setColumn(0);
            m_series->setName("Styled");
            m_series->setColor(QColor(kSeriesRed));
            m_series->setData(std::move(points));
        }

        [[nodiscard]] presentation::SeriesModel &model() const { return *m_series; }
        [[nodiscard]] QList<presentation::SeriesModel *> series(const QList<int> &columns) const override {
            return columns.contains(0) ? QList<presentation::SeriesModel *>{m_series}
                                       : QList<presentation::SeriesModel *>{};
        }
        [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
        [[nodiscard]] int yAxisColumn() const override { return 0; }

        [[nodiscard]] QPointF pixelOf(const GraphCanvas &canvas, const QPointF &raw) const {
            const QRectF plot = canvas.property("plotArea").toRectF();
            return toPixel(QPointF(raw.x(), m_series->transform.display(raw.y())), plot, m_xAxis,
                           m_series->yAxis.value_or(m_series->deriveYAxis()));
        }

    private:
        presentation::ValueAxis m_xAxis;
        presentation::SeriesModel *m_series;
    };

    QImage render(GraphCanvas &canvas) {
        QImage image(int(canvas.width()), int(canvas.height()), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        canvas.paint(&painter);
        return image;
    }

    int countColour(const QImage &image, const QRect &area, const QRgb colour) {
        int count = 0;
        const QRect bounded = area.intersected(image.rect());
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            for (int x = bounded.left(); x <= bounded.right(); ++x) {
                const QColor c = image.pixelColor(x, y);
                if (c.alpha() > 200 && std::abs(c.red() - qRed(colour)) < 40 && std::abs(c.green() - qGreen(colour)) < 40 &&
                    std::abs(c.blue() - qBlue(colour)) < 40)
                    ++count;
            }
        }
        return count;
    }

    TEST(GraphCanvasConnectionTest, ConnectionModeDefaultsToSmoothAndKeepsTheMarkerPreference) {
        GraphCanvas canvas;
        EXPECT_EQ(canvas.property("connectionMode").toInt(), int(GraphCanvas::ConnectionMode::Smooth));

        int changes = 0;
        QObject::connect(&canvas, &GraphCanvas::connectionModeChanged, [&changes] { ++changes; });
        canvas.setPointMarkers(true);
        canvas.setProperty("connectionMode", int(GraphCanvas::ConnectionMode::PointsOnly));
        canvas.setConnectionMode(GraphCanvas::ConnectionMode::PointsOnly);

        EXPECT_EQ(changes, 1);
        EXPECT_TRUE(canvas.pointMarkers()) << "Points only never overwrites the line-mode marker preference";
    }

    TEST(GraphCanvasConnectionTest, PointsOnlyBypassesMarkerSpacingAndRestoresThePreferenceAfterwards) {
        RunsVm many(224);
        const auto canvas = makeCanvas(many, QVariantList{0});

        for (const bool preference: {false, true}) {
            canvas->setPointMarkers(preference);
            canvas->setConnectionMode(GraphCanvas::ConnectionMode::Smooth);
            EXPECT_FALSE(canvas->pointMarkersShown()) << "dense series stay plain lines in line modes";

            canvas->setConnectionMode(GraphCanvas::ConnectionMode::PointsOnly);
            EXPECT_TRUE(canvas->pointMarkersShown()) << "Points only forces markers on regardless of density";
            EXPECT_EQ(canvas->pointMarkers(), preference);

            canvas->setConnectionMode(GraphCanvas::ConnectionMode::Straight);
            EXPECT_FALSE(canvas->pointMarkersShown());
            EXPECT_EQ(canvas->pointMarkers(), preference);
        }

        RunsVm few(8);
        const auto fewCanvas = makeCanvas(few, QVariantList{0});
        EXPECT_FALSE(fewCanvas->pointMarkersShown());
        fewCanvas->setConnectionMode(GraphCanvas::ConnectionMode::PointsOnly);
        EXPECT_TRUE(fewCanvas->pointMarkersShown()) << "the preference being off does not hide Points only";
        fewCanvas->setConnectionMode(GraphCanvas::ConnectionMode::Smooth);
        EXPECT_FALSE(fewCanvas->pointMarkersShown()) << "back to the stored preference";
    }

    TEST(GraphCanvasConnectionTest, PointsOnlyPaintsMarkersAndNoConnection) {
        QList<QPointF> points;
        for (int i = 0; i <= 10; ++i) points.append(QPointF(i, i % 2 == 0 ? 10.0 : 90.0));
        StyledVm vm(points, 0.0, 10.0);
        const auto canvas = makeCanvas(vm, QVariantList{0});

        const QImage smooth = render(*canvas);
        canvas->setConnectionMode(GraphCanvas::ConnectionMode::PointsOnly);
        const QImage pointsOnly = render(*canvas);

        const QPointF first = vm.pixelOf(*canvas, points[0]);
        const QPointF second = vm.pixelOf(*canvas, points[1]);
        const QRect betweenPoints(int((first.x() + second.x()) / 2) - 1, int(std::min(first.y(), second.y())) + 8, 3,
                                  int(std::abs(first.y() - second.y())) - 16);
        EXPECT_GT(countColour(smooth, betweenPoints, kSeriesRed), 0) << "the line crosses between the points";
        EXPECT_EQ(countColour(pointsOnly, betweenPoints, kSeriesRed), 0) << "Points only has no connection";
        EXPECT_GT(countColour(pointsOnly, QRect(int(first.x()) - 1, int(first.y()) - 1, 3, 3), kSeriesRed), 7)
            << "markers remain";
    }

    TEST(GraphCanvasConnectionTest, PointsOnlyKeepsTierReferenceLinesAndSkipsCarriedGeometry) {
        StyledVm vm({QPointF(5.0, 40.0)}, 0.0, 10.0);
        vm.model().leadingCarry = QPointF(0.0, 40.0);
        vm.model().trailingCarry = QPointF(10.0, 40.0);
        vm.model().referenceLines = {presentation::ReferenceLine{.value = 10.0, .color = QColor(kReferenceBlue)}};
        presentation::ValueAxis fixed;
        fixed.setRange(0.0, 50.0);
        vm.model().yAxis = fixed;
        const auto canvas = makeCanvas(vm, QVariantList{0});
        const QRectF plot = canvas->property("plotArea").toRectF();
        const QPointF observation = vm.pixelOf(*canvas, QPointF(5.0, 40.0));
        const QRect carriedStretch(int(plot.left()) + 4, int(observation.y()) - 2, int(observation.x() - plot.left()) - 20, 5);
        const QRect wholePlot(int(plot.left()), int(plot.top()), int(plot.width()), int(plot.height()));

        canvas->setConnectionMode(GraphCanvas::ConnectionMode::Smooth);
        const QImage smooth = render(*canvas);
        canvas->setConnectionMode(GraphCanvas::ConnectionMode::PointsOnly);
        const QImage pointsOnly = render(*canvas);

        EXPECT_GT(countColour(smooth, carriedStretch, kSeriesRed), 20) << "line modes draw the carried level";
        EXPECT_EQ(countColour(pointsOnly, carriedStretch, kSeriesRed), 0);
        EXPECT_GT(countColour(pointsOnly, QRect(int(observation.x()) - 4, int(observation.y()) - 4, 9, 9), kSeriesRed), 0);
        EXPECT_GT(countColour(smooth, wholePlot, kReferenceBlue), 200);
        EXPECT_EQ(countColour(pointsOnly, wholePlot, kReferenceBlue), countColour(smooth, wholePlot, kReferenceBlue))
            << "reference lines keep their own path in every mode";
    }

    TEST(GraphCanvasConnectionTest, ALoneObservationWithCarryIsMarkedEvenWithMarkersOff) {
        StyledVm vm({QPointF(5.0, 40.0)}, 0.0, 10.0);
        vm.model().leadingCarry = QPointF(0.0, 40.0);
        vm.model().trailingCarry = QPointF(10.0, 40.0);
        const auto canvas = makeCanvas(vm, QVariantList{0});
        vm.model().lineWidth = 2.0;
        const QPointF observation = vm.pixelOf(*canvas, QPointF(5.0, 40.0));

        for (const auto mode: {GraphCanvas::ConnectionMode::Smooth, GraphCanvas::ConnectionMode::Straight}) {
            canvas->setConnectionMode(mode);
            const QImage image = render(*canvas);
            EXPECT_GE(countColour(image, QRect(int(observation.x()), int(observation.y()) - 4, 1, 9), kSeriesRed), 4)
                << "the marker is taller than the 2px line";
            EXPECT_GT(countColour(image, QRect(int(observation.x()) - 30, int(observation.y()) - 1, 3, 3), kSeriesRed), 0)
                << "and the carried level is still drawn";
        }
    }

    TEST(GraphCanvasConnectionTest, SeriesLineWidthReachesThePainter) {
        StyledVm vm({QPointF(0.0, 50.0), QPointF(10.0, 50.0)}, 0.0, 10.0);
        const auto canvas = makeCanvas(vm, QVariantList{0});
        const QPointF middle = vm.pixelOf(*canvas, QPointF(5.0, 50.0));
        const QRect column(int(middle.x()), int(middle.y()) - 20, 1, 41);

        const int defaultWidth = countColour(render(*canvas), column, kSeriesRed);
        vm.model().lineWidth = 9.0;
        const int thick = countColour(render(*canvas), column, kSeriesRed);
        vm.model().lineWidth = 1.0;
        const int thin = countColour(render(*canvas), column, kSeriesRed);

        EXPECT_NEAR(defaultWidth, 3, 1);
        EXPECT_NEAR(thick, 9, 1);
        EXPECT_NEAR(thin, 1, 1);
    }

    TEST(GraphCanvasConnectionTest, FixedBoundsClipDataGeometryToThePlotArea) {
        StyledVm vm({QPointF(0.0, 10.0), QPointF(5.0, 400.0), QPointF(10.0, 10.0)}, 0.0, 10.0);
        presentation::ValueAxis fixed;
        fixed.setRange(0.0, 50.0);
        vm.model().yAxis = fixed;
        const auto canvas = makeCanvas(vm, QVariantList{0});
        canvas->setPointMarkers(true);
        canvas->setConnectionMode(GraphCanvas::ConnectionMode::Straight);
        const QRectF plot = canvas->property("plotArea").toRectF();

        const QImage image = render(*canvas);

        const int slack = 3;
        EXPECT_GT(countColour(image, QRect(0, int(plot.top()), image.width(), int(plot.height())), kSeriesRed), 50);
        EXPECT_EQ(countColour(image, QRect(0, 0, image.width(), int(plot.top()) - slack), kSeriesRed), 0)
            << "nothing above the plot, neither line nor the out-of-bounds marker";
        EXPECT_EQ(countColour(image, QRect(0, int(plot.bottom()) + slack, image.width(), image.height()), kSeriesRed), 0);
    }

    TEST(GraphCanvasConnectionTest, SplineAndCarriedGeometryAreNeverSelectable) {
        QList<QPointF> points{QPointF(2.0, 20.0), QPointF(5.0, 80.0), QPointF(8.0, 30.0)};
        StyledVm vm(points, 0.0, 10.0);
        vm.model().leadingCarry = QPointF(0.0, 20.0);
        vm.model().trailingCarry = QPointF(10.0, 30.0);
        const auto canvas = makeCanvas(vm, QVariantList{0});
        canvas->setPointMarkers(true);

        for (const auto mode: {GraphCanvas::ConnectionMode::Smooth, GraphCanvas::ConnectionMode::Straight}) {
            canvas->setConnectionMode(mode);
            const QPointF leadingCarry = vm.pixelOf(*canvas, *vm.model().leadingCarry);
            EXPECT_FALSE(canvas->nearestPoint(leadingCarry.x(), leadingCarry.y()).value("valid").toBool());
            const QPointF trailingCarry = vm.pixelOf(*canvas, *vm.model().trailingCarry);
            EXPECT_FALSE(canvas->nearestPoint(trailingCarry.x(), trailingCarry.y()).value("valid").toBool());

            const QPointF a = vm.pixelOf(*canvas, points[0]);
            const QPointF b = vm.pixelOf(*canvas, points[1]);
            const QPointF midway = (a + b) / 2.0;
            EXPECT_FALSE(canvas->nearestPoint(midway.x(), midway.y()).value("valid").toBool())
                << "the curve between two observations is not a data point";

            const QVariantMap hover = canvas->valuesAtX(leadingCarry.x());
            ASSERT_TRUE(hover.value("valid").toBool());
            EXPECT_DOUBLE_EQ(hover.value("xRaw").toReal(), 2.0) << "hover snaps to an observation, never the carry";
        }
    }
}
