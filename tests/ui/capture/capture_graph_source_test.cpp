#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <variant>

#include <QFont>
#include <QGuiApplication>
#include <QList>
#include <QPointF>
#include <QStringList>

#include "capture/capture_sources.h"
#include "capture/graph_capture_layout.h"
#include "components/graph_canvas.h"
#include "presentation/graphs/common/axes/value_axis.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"

namespace ksv::ui {
    namespace {
        using presentation::SeriesModel;
        using presentation::ValueAxis;

        class SourceVm : public presentation::GraphViewModelBase {
        public:
            SourceVm() { m_xAxis.setRange(0.0, 10.0); }

            SeriesModel &addSeries(const int column, const QString &name) {
                auto *series = new SeriesModel(this);
                series->setColumn(column);
                series->setId(QStringLiteral("id-%1").arg(column));
                series->setName(name);
                m_series.append(series);
                return *series;
            }

            [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override {
                QList<SeriesModel *> result;
                for (auto *series: m_series) {
                    if (columns.contains(series->column())) result.append(series);
                }
                return result;
            }

            [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
            [[nodiscard]] int yAxisColumn() const override { return 1; }

        private:
            ValueAxis m_xAxis;
            QList<SeriesModel *> m_series;
        };

        void fillThreeSeries(SourceVm &vm) {
            vm.addSeries(1, QStringLiteral("One")).setData({QPointF(0.0, 10.0), QPointF(5.0, 50.0)});
            SeriesModel &two = vm.addSeries(2, QStringLiteral("Two"));
            two.setData({QPointF(1.0, 20.0), QPointF(6.0, 60.0)});
            two.lineWidth = 4.0;
            two.setColor(QColor(Qt::green));
            vm.addSeries(3, QStringLiteral("Three")).setData({QPointF(2.0, 30.0), QPointF(7.0, 70.0)});
        }

        void expectSameSeries(const QList<GraphSceneSeries> &actual, const QList<GraphSceneSeries> &expected) {
            ASSERT_EQ(actual.size(), expected.size());
            for (qsizetype i = 0; i < expected.size(); ++i) {
                EXPECT_EQ(actual.at(i).column, expected.at(i).column);
                EXPECT_EQ(actual.at(i).id, expected.at(i).id);
                EXPECT_EQ(actual.at(i).name, expected.at(i).name);
                EXPECT_EQ(actual.at(i).color, expected.at(i).color);
                EXPECT_EQ(actual.at(i).lineWidth, expected.at(i).lineWidth);
                EXPECT_EQ(actual.at(i).observations, expected.at(i).observations);
                EXPECT_EQ(actual.at(i).leadingCarry, expected.at(i).leadingCarry);
                EXPECT_EQ(actual.at(i).trailingCarry, expected.at(i).trailingCarry);
            }
        }

        std::optional<GraphCaptureSnapshot> capture(const CaptureTarget target, const GraphCanvas &canvas) {
            return captureGraph(target, canvas, GraphCaptureContext{.title = QStringLiteral("Title")},
                                CaptureAppearance{});
        }
    }

    TEST(CaptureGraphSource, CopiesTheSceneTheCanvasPaints) {
        SourceVm vm;
        fillThreeSeries(vm);
        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({3, 2});
        canvas.setYAxisColumn(2);
        canvas.setConnectionMode(GraphCanvas::ConnectionMode::Straight);
        canvas.setPointMarkers(true);
        canvas.setMarkedX(5.0);
        const GraphScene live = canvas.scene();
        ASSERT_TRUE(live.selectedBand.has_value());

        const auto snapshot = capture(CaptureTarget::Run, canvas);

        ASSERT_TRUE(snapshot.has_value());
        EXPECT_EQ(snapshot->target, CaptureTarget::Run);
        EXPECT_EQ(snapshot->context.title, QStringLiteral("Title"));
        expectSameSeries(snapshot->scene.series, live.series);
        EXPECT_EQ(snapshot->scene.connection, live.connection);
        EXPECT_EQ(snapshot->scene.pointMarkers, live.pointMarkers);
        EXPECT_EQ(snapshot->scene.labelledColumn, live.labelledColumn);
        EXPECT_EQ(snapshot->scene.labelledYAxisTitle, live.labelledYAxisTitle);
        EXPECT_FALSE(snapshot->scene.selectedBand.has_value());
        EXPECT_TRUE(canvas.scene().selectedBand.has_value());
    }

    TEST(CaptureGraphSource, SurvivesTheCanvasAndViewModel) {
        auto vm = std::make_unique<SourceVm>();
        fillThreeSeries(*vm);
        auto canvas = std::make_unique<GraphCanvas>();
        canvas->setGraphVm(vm.get());
        canvas->setVisibleColumns({1, 2});
        const QList<GraphSceneSeries> before = canvas->scene().series;

        const auto snapshot = capture(CaptureTarget::Run, *canvas);
        ASSERT_TRUE(snapshot.has_value());

        vm->series({1}).front()->setData({QPointF(9.0, 99.0)});
        canvas.reset();
        vm.reset();

        expectSameSeries(snapshot->scene.series, before);
    }

    TEST(CaptureGraphSource, ReportsUnavailableContent) {
        const QPointF leading(-1.0, 5.0);
        const QPointF trailing(11.0, 7.0);
        const auto carriedOnly = [&](const GraphCanvas::ConnectionMode mode, SourceVm &vm, GraphCanvas &canvas) {
            SeriesModel &series = vm.addSeries(1, QStringLiteral("Carried"));
            series.leadingCarry = leading;
            series.trailingCarry = trailing;
            canvas.setGraphVm(&vm);
            canvas.setVisibleColumns({1});
            canvas.setConnectionMode(mode);
        };

        SourceVm lineVm;
        GraphCanvas lineCanvas;
        carriedOnly(GraphCanvas::ConnectionMode::Straight, lineVm, lineCanvas);
        const auto line = capture(CaptureTarget::Run, lineCanvas);
        ASSERT_TRUE(line.has_value());
        ASSERT_EQ(line->scene.series.size(), 1);
        EXPECT_TRUE(line->scene.series.at(0).observations.isEmpty());
        EXPECT_EQ(line->scene.series.at(0).leadingCarry, std::optional<QPointF>(leading));
        EXPECT_EQ(line->scene.series.at(0).trailingCarry, std::optional<QPointF>(trailing));

        GraphCanvas noVmCanvas;
        EXPECT_FALSE(capture(CaptureTarget::Run, noVmCanvas).has_value());

        SourceVm hiddenVm;
        fillThreeSeries(hiddenVm);
        GraphCanvas hiddenCanvas;
        hiddenCanvas.setGraphVm(&hiddenVm);
        hiddenCanvas.setVisibleColumns({});
        EXPECT_FALSE(capture(CaptureTarget::Run, hiddenCanvas).has_value());

        SourceVm pointsVm;
        GraphCanvas pointsCanvas;
        carriedOnly(GraphCanvas::ConnectionMode::PointsOnly, pointsVm, pointsCanvas);
        EXPECT_FALSE(capture(CaptureTarget::Run, pointsCanvas).has_value());
    }

    TEST(CaptureGraphSource, ManyTiersAreRefusedForRankTargetsButThinnedForOthers) {
        constexpr int kTiers = 60;
        SourceVm vm;
        SeriesModel &series = vm.addSeries(1, QStringLiteral("Tiers"));
        series.setData({QPointF(0.0, 1.0), QPointF(5.0, kTiers - 1.0)});
        ValueAxis::Options options;
        options.baseline = ValueAxis::Baseline::Zero;
        options.integral = true;
        options.targetTicks = 2 * kTiers;
        series.yAxis = ValueAxis(options);
        series.yAxis->setRange(0.0, kTiers - 1.0);
        GraphCanvas canvas;
        canvas.setGraphVm(&vm);
        canvas.setVisibleColumns({1});

        const CaptureComposition composition = standardCaptureComposition();
        CaptureAppearance appearance;
        const QFont applicationFont = QGuiApplication::font();
        appearance.font.families = applicationFont.families().isEmpty() ? QStringList{applicationFont.family()}
                                                                        : applicationFont.families();
        appearance.font.pixelSize = 14;
        const auto layoutAtStandard = [&](const CaptureTarget target) {
            const auto snapshot = captureGraph(target, canvas, GraphCaptureContext{.title = QStringLiteral("Title")},
                                               appearance);
            EXPECT_TRUE(snapshot.has_value());
            return layoutGraphCapture(*snapshot, composition.standardGraph, composition);
        };

        EXPECT_TRUE(std::holds_alternative<CaptureRejection>(layoutAtStandard(CaptureTarget::BenchmarkRank)));
        EXPECT_TRUE(std::holds_alternative<CaptureRejection>(layoutAtStandard(CaptureTarget::BenchmarkRecentRank)));
        EXPECT_TRUE(std::holds_alternative<CaptureLayout>(layoutAtStandard(CaptureTarget::Run)));
    }
}
