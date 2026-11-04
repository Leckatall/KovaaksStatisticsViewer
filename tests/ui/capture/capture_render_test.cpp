#include <gtest/gtest.h>

#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QPointF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "capture/capture_limits.h"
#include "capture/capture_render.h"
#include "capture/capture_snapshot.h"
#include "capture/graph_capture_layout.h"
#include "components/series_painter.h"
#include "table_capture_fixtures.h"

namespace ksv::ui {
    namespace {
        GraphCaptureSnapshot graphSnapshot() {
            GraphSceneAxis xAxis;
            xAxis.min = 0.0;
            xAxis.max = 10.0;
            xAxis.ticks = {0.0, 10.0};
            xAxis.labels = {QStringLiteral("0"), QStringLiteral("10")};
            xAxis.minLabel = QStringLiteral("0");
            xAxis.maxLabel = QStringLiteral("10");

            GraphSceneAxis yAxis = xAxis;
            yAxis.max = 100.0;
            yAxis.ticks = {0.0, 100.0};
            yAxis.labels = {QStringLiteral("0"), QStringLiteral("100")};
            yAxis.maxLabel = QStringLiteral("100");

            GraphSceneSeries line;
            line.column = 1;
            line.id = QStringLiteral("series-1");
            line.name = QStringLiteral("Damage");
            line.color = Qt::red;
            line.lineWidth = 2.0;
            line.observations = {QPointF(0.0, 0.0), QPointF(5.0, 100.0), QPointF(10.0, 0.0)};
            line.xAxis = xAxis;
            line.yAxis = yAxis;

            GraphCaptureSnapshot snapshot;
            snapshot.target = CaptureTarget::Run;
            snapshot.context.title = QStringLiteral("Pasu Track Invincible");
            snapshot.context.graphType = QStringLiteral("Run graph");
            snapshot.context.rangeText = QStringLiteral("Elapsed 0:00 to 1:00");
            snapshot.context.xTitle = QStringLiteral("Elapsed seconds");
            snapshot.appearance.window = QColor(16, 24, 32);
            snapshot.appearance.text = QColor(240, 240, 240);
            snapshot.appearance.placeholderText = QColor(160, 160, 160);
            const QFont app = QGuiApplication::font();
            snapshot.appearance.font.families = app.families().isEmpty() ? QStringList{app.family()} : app.families();
            snapshot.appearance.font.pixelSize = 14;
            snapshot.scene.series.append(line);
            snapshot.scene.xAxis = xAxis;
            snapshot.scene.labelledYAxis = yAxis;
            snapshot.scene.labelledColumn = 1;
            snapshot.scene.labelledColumns = {1};
            snapshot.scene.labelledYAxisTitle = QStringLiteral("Damage done");
            snapshot.scene.connection = SeriesPainter::Connection::Straight;
            snapshot.scene.background = snapshot.appearance.window;
            return snapshot;
        }

        // Small enough that twelve tier labels fit 300 logical pixels of height but not 100.
        GraphCaptureSnapshot rankedSnapshot() {
            GraphCaptureSnapshot snapshot = graphSnapshot();
            GraphSceneAxis yAxis;
            yAxis.min = 0.0;
            yAxis.max = 11.0;
            for (int tier = 0; tier < 12; ++tier) {
                yAxis.ticks.append(static_cast<qreal>(tier));
                yAxis.labels.append(QStringLiteral("Tier %1").arg(tier + 1));
            }
            yAxis.minLabel = yAxis.labels.first();
            yAxis.maxLabel = yAxis.labels.last();
            snapshot.scene.series[0].yAxis = yAxis;
            snapshot.scene.series[0].observations = {QPointF(0.0, 0.0), QPointF(10.0, 11.0)};
            snapshot.scene.labelledYAxis = yAxis;
            snapshot.keepEveryYLabel = true;
            snapshot.appearance.font.pixelSize = 6;
            return snapshot;
        }

        TableCaptureSnapshot tableSnapshotWithSummary() {
            TableCaptureSnapshot snapshot = largeTableFixture();
            snapshot.summary = tableFixtureSummary(snapshot.grid.tiers);
            return snapshot;
        }

        CaptureRenderRequest requestFor(CaptureContent content, CaptureOptions options = {}) {
            return {1, 1, std::make_shared<const CaptureContent>(std::move(content)), std::move(options)};
        }
    }

    TEST(CaptureRender, ACancelledRenderIsNeverASuccess) {
        const CaptureRenderFunction render =
                makeCaptureRenderFunction(standardCaptureLimits(), standardCaptureComposition());
        const auto cancelled = std::make_shared<std::atomic_bool>(true);
        CaptureRenderRequest graph = requestFor(graphSnapshot());
        graph.cancelled = cancelled;
        CaptureRenderRequest table = requestFor(tableSnapshotWithSummary());
        table.cancelled = cancelled;

        EXPECT_TRUE(std::holds_alternative<CaptureRenderFailed>(render(graph)));
        EXPECT_TRUE(std::holds_alternative<CaptureRenderFailed>(render(table)));
    }

    TEST(CaptureRender, AGraphRendersAtItsReferenceTimesTheResolution) {
        const CaptureComposition composition = standardCaptureComposition();
        const CaptureRenderFunction render = makeCaptureRenderFunction(standardCaptureLimits(), composition);
        const GraphCaptureSnapshot snapshot = graphSnapshot();

        const CaptureRenderOutcome single = render(requestFor(snapshot, {.percent = 100}));
        const CaptureRenderOutcome twice = render(requestFor(snapshot, {.percent = 200}));

        const auto *one = std::get_if<CaptureRendered>(&single);
        const auto *two = std::get_if<CaptureRendered>(&twice);
        ASSERT_NE(one, nullptr);
        ASSERT_NE(two, nullptr);
        EXPECT_EQ(two->pixels, one->pixels * 2);
        EXPECT_EQ(two->logical, graphReferenceSize(snapshot, composition));
        EXPECT_EQ(QSize(two->raster.width(), two->raster.height()), two->pixels);
    }

    TEST(CaptureRender, ATableFollowsItsSummaryChoice) {
        const CaptureRenderFunction render =
                makeCaptureRenderFunction(standardCaptureLimits(), standardCaptureComposition());
        const TableCaptureSnapshot snapshot = tableSnapshotWithSummary();

        const CaptureRenderOutcome with = render(requestFor(snapshot, {.includeSummary = true}));
        const CaptureRenderOutcome without = render(requestFor(snapshot, {.includeSummary = false}));

        const auto *withSummary = std::get_if<CaptureRendered>(&with);
        const auto *withoutSummary = std::get_if<CaptureRendered>(&without);
        ASSERT_NE(withSummary, nullptr);
        ASSERT_NE(withoutSummary, nullptr);
        EXPECT_EQ(withoutSummary->pixels.width(), withSummary->pixels.width());
        EXPECT_LT(withoutSummary->pixels.height(), withSummary->pixels.height());
    }

    TEST(CaptureRender, ACustomLogicalSizeIsHonoured) {
        const CaptureRenderFunction render =
                makeCaptureRenderFunction(standardCaptureLimits(), standardCaptureComposition());

        const CaptureRenderOutcome outcome =
                render(requestFor(graphSnapshot(), {.percent = 200, .logical = QSizeF(900.0, 500.0)}));

        const auto *rendered = std::get_if<CaptureRendered>(&outcome);
        ASSERT_NE(rendered, nullptr);
        EXPECT_EQ(rendered->pixels, QSize(1800, 1000));
    }

    TEST(CaptureRender, AnOversizedOutputIsRejectedBeforePainting) {
        CaptureLimits limits = standardCaptureLimits();
        limits.maxEdgePixels = 500;
        const CaptureComposition composition = standardCaptureComposition();
        const CaptureRenderFunction render = makeCaptureRenderFunction(limits, composition);
        const GraphCaptureSnapshot snapshot = graphSnapshot();

        const CaptureRenderOutcome outcome = render(requestFor(snapshot));

        const auto *rejected = std::get_if<CaptureRejected>(&outcome);
        ASSERT_NE(rejected, nullptr);
        EXPECT_EQ(rejected->rejection.reason, CaptureRejectionReason::EdgeLimit);
        EXPECT_EQ(rejected->logical, graphReferenceSize(snapshot, composition));
    }

    TEST(CaptureRender, TooLittleRoomNamesAValidatedResolution) {
        const CaptureLimits limits = standardCaptureLimits();
        CaptureComposition composition = standardCaptureComposition();
        composition.minimumPlot = QSizeF(100.0, 20.0);
        const CaptureRenderFunction render = makeCaptureRenderFunction(limits, composition);
        const GraphCaptureSnapshot snapshot = rankedSnapshot();
        const QSize pixels(960, 300);

        const CaptureRenderOutcome outcome =
                render(requestFor(snapshot, {.percent = 300, .logical = QSizeF(320.0, 100.0)}));

        const auto *rejected = std::get_if<CaptureRejected>(&outcome);
        ASSERT_NE(rejected, nullptr);
        EXPECT_EQ(rejected->rejection.reason, CaptureRejectionReason::TooLittleRoom);
        const std::optional<CaptureRejection> expected =
                suggestGraphResolution(snapshot, pixels, 300, composition, limits);
        ASSERT_TRUE(expected.has_value());
        ASSERT_TRUE(expected->suggestedPercent.has_value());
        EXPECT_EQ(rejected->rejection.suggestedPercent, expected->suggestedPercent);
    }

    TEST(CaptureRender, TheThumbnailStaysWithinItsAllowance) {
        CaptureLimits limits = standardCaptureLimits();
        limits.thumbnailBytes = 64 * 1024;
        const CaptureRenderFunction render = makeCaptureRenderFunction(limits, standardCaptureComposition());

        const CaptureRenderOutcome outcome = render(requestFor(graphSnapshot()));

        const auto *rendered = std::get_if<CaptureRendered>(&outcome);
        ASSERT_NE(rendered, nullptr);
        ASSERT_GT(static_cast<qint64>(rendered->raster.byteSize()), limits.thumbnailBytes);
        ASSERT_FALSE(rendered->thumbnail.isNull());
        EXPECT_LE(rendered->thumbnail.sizeInBytes(), limits.thumbnailBytes);
        const qreal rasterAspect = static_cast<qreal>(rendered->raster.width()) / rendered->raster.height();
        EXPECT_NEAR(rendered->thumbnail.width(), rendered->thumbnail.height() * rasterAspect, 1.0 + rasterAspect);
        EXPECT_NE(static_cast<const void *>(rendered->thumbnail.constBits()),
                  static_cast<const void *>(rendered->raster.pixels()));
    }
}
