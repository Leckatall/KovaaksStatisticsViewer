#include <gtest/gtest.h>

#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>

#include <climits>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include "app/contracts/raster_image.h"
#include "capture/capture_fonts.h"
#include "capture/capture_layout.h"
#include "capture/capture_renderer.h"
#include "capture/capture_sizing.h"
#include "capture_test_support.h"
#include "components/axis_painter.h"
#include "components/graph_scene_painter.h"

namespace ksv::ui {
    namespace {
        const QColor kBackground(16, 24, 32);
        const QColor kForeground(240, 240, 240);
        const QColor kSeriesColour(Qt::red);

        CaptureFontDescription applicationDescription() {
            const QFont app = QGuiApplication::font();
            CaptureFontDescription description;
            description.families = app.families().isEmpty() ? QStringList{app.family()} : app.families();
            description.pixelSize = 14;
            return description;
        }

        CaptureLayout emptyLayout(const QSizeF size) {
            CaptureLayout layout;
            layout.size = size;
            layout.background = kBackground;
            layout.font = applicationDescription();
            return layout;
        }

        CaptureTextItem textItem(const QRectF &rect, const QString &text) {
            CaptureTextItem item;
            item.rect = rect;
            item.text = text;
            item.role = CaptureTextRole::Body;
            item.color = kForeground;
            item.alignment = Qt::AlignLeft | Qt::AlignVCenter;
            return item;
        }

        GraphScene oneSeriesScene() {
            const GraphSceneAxis xAxis{0.0, 10.0, {0.0, 10.0}, {QStringLiteral("0"), QStringLiteral("10")}};
            const GraphSceneAxis yAxis{0.0, 100.0, {0.0, 100.0}, {QStringLiteral("0"), QStringLiteral("100")}};

            GraphSceneSeries series;
            series.column = 1;
            series.color = kSeriesColour;
            series.lineWidth = 3.0;
            series.observations = {QPointF(0.0, 20.0), QPointF(5.0, 80.0), QPointF(10.0, 50.0)};
            series.xAxis = xAxis;
            series.yAxis = yAxis;

            GraphScene scene;
            scene.series = {series};
            scene.xAxis = xAxis;
            scene.labelledYAxis = yAxis;
            scene.labelledColumn = series.column;
            scene.connection = SeriesPainter::Connection::Straight;
            scene.background = kBackground;
            return scene;
        }

        CaptureLayout textAndGraphLayout() {
            CaptureLayout layout = emptyLayout(QSizeF(300.0, 160.0));
            layout.items.emplace_back(textItem(QRectF(10.0, 4.0, 280.0, 20.0), QStringLiteral("Off-thread title")));
            layout.items.emplace_back(CaptureGraphItem{QRectF(10.0, 30.0, 280.0, 120.0)});
            layout.scene = std::make_shared<const GraphScene>(oneSeriesScene());
            return layout;
        }

        std::optional<application::RasterImage> renderAt(const CaptureLayout &layout, const int percent) {
            return renderCapture(layout, captureOutputPixels(layout.size, percent).value());
        }

        // Valid only while `raster` lives.
        QImage imageView(const application::RasterImage &raster) {
            return QImage(reinterpret_cast<const uchar *>(raster.pixels()), raster.width(), raster.height(),
                          static_cast<qsizetype>(raster.stride()), QImage::Format_RGB32);
        }
    }

    TEST(CaptureRenderer, ProducesExactlyTheRequestedPixels) {
        CaptureLayout layout = emptyLayout(QSizeF(300.0, 120.0));
        CaptureRectItem rect;
        rect.rect = QRectF(40.0, 30.0, 120.0, 40.0);
        rect.fill = kForeground;
        layout.items.emplace_back(rect);
        layout.items.emplace_back(textItem(QRectF(40.0, 80.0, 200.0, 24.0), QStringLiteral("Exact dimensions")));

        for (const int percent : {100, 200, 350}) {
            const QSize pixels = captureOutputPixels(layout.size, percent).value();

            const std::optional<application::RasterImage> raster = renderCapture(layout, pixels);

            ASSERT_TRUE(raster.has_value()) << percent;
            EXPECT_EQ(raster->width(), pixels.width()) << percent;
            EXPECT_EQ(raster->height(), pixels.height()) << percent;
            EXPECT_EQ(imageView(*raster).pixelColor(0, 0), kBackground) << percent;
        }
    }

    // Full-colour pixels are where a resample and a fresh rasterization differ most: glyph stems and thin strokes
    // antialias to partial coverage at 100%, and enlarging can only blur that coverage, never restore it.
    TEST(CaptureRenderer, HigherResolutionHasMoreTextDetailThanEnlarging) {
        CaptureLayout layout = emptyLayout(QSizeF(240.0, 80.0));
        layout.items.emplace_back(textItem(QRectF(10.0, 4.0, 220.0, 24.0), QStringLiteral("Resolution detail")));
        CaptureGlyphItem line;
        line.kind = CaptureGlyphItem::Kind::LineSample;
        line.rect = QRectF(10.0, 34.0, 220.0, 40.0);
        line.color = kForeground;
        line.lineWidth = 1.0;
        layout.items.emplace_back(line);

        const std::optional<application::RasterImage> fresh = renderAt(layout, 400);
        const std::optional<application::RasterImage> reference = renderAt(layout, 100);

        ASSERT_TRUE(fresh.has_value());
        ASSERT_TRUE(reference.has_value());
        const QImage freshImage = imageView(*fresh);
        const QImage enlarged = imageView(*reference).scaled(freshImage.size(), Qt::IgnoreAspectRatio,
                                                             Qt::SmoothTransformation);
        const qint64 freshCount = countPixelsAtColor(freshImage, kForeground);
        const qint64 enlargedCount = countPixelsAtColor(enlarged, kForeground);
        EXPECT_GT(freshCount, 2 * enlargedCount);
    }

    TEST(CaptureRenderer, PaintsTheGraphItemThroughTheSharedPath) {
        const QRectF graphRect(20.0, 10.0, 260.0, 140.0);
        CaptureLayout layout = emptyLayout(QSizeF(300.0, 160.0));
        layout.items.emplace_back(CaptureGraphItem{graphRect});
        layout.scene = std::make_shared<const GraphScene>(oneSeriesScene());

        const std::optional<application::RasterImage> raster = renderAt(layout, 100);

        ASSERT_TRUE(raster.has_value());
        QImage reference(raster->width(), raster->height(), QImage::Format_RGB32);
        reference.fill(kBackground);
        {
            AxisPainter::Style style;
            style.font = captureFont(layout.font, CaptureTextRole::Tick);
            QPainter painter(&reference);
            painter.setRenderHint(QPainter::TextAntialiasing, true);
            paintGraphScene(painter, *layout.scene, graphRect, style);
        }
        ASSERT_GT(countPixelsAtColor(reference, kSeriesColour), 0);
        const QRect inside = graphRect.toAlignedRect();
        EXPECT_TRUE(imageView(*raster).copy(inside) == reference.copy(inside));
    }

    TEST(CaptureRenderer, RunsOffTheGuiThread) {
        const CaptureLayout layout = textAndGraphLayout();

        std::optional<application::RasterImage> worker;
        std::thread thread([&] { worker = renderAt(layout, 200); });
        thread.join();
        const std::optional<application::RasterImage> main = renderAt(layout, 200);

        ASSERT_TRUE(worker.has_value());
        ASSERT_TRUE(main.has_value());
        ASSERT_EQ(worker->byteSize(), main->byteSize());
        EXPECT_EQ(std::memcmp(worker->pixels(), main->pixels(), main->byteSize()), 0);
        EXPECT_LT(countPixelsAtColor(imageView(*main), kBackground), qint64(main->width()) * main->height());
    }

    TEST(CaptureRenderer, AnImpossibleAllocationIsACaughtFailure) {
        if constexpr (sizeof(std::size_t) == 8) {
            ASSERT_FALSE(application::RasterBuffer::allocate(INT_MAX, INT_MAX).has_value());
            const CaptureLayout layout = emptyLayout(QSizeF(300.0, 120.0));

            std::optional<application::RasterImage> raster;
            EXPECT_NO_THROW(raster = renderCapture(layout, QSize(INT_MAX, INT_MAX)));

            EXPECT_FALSE(raster.has_value());
        }
    }

    // Series are painted back to front, so the last series in the list is the first to reach the image.
    TEST(CaptureRenderer, CancellingMidGraphStopsBeforeTheRemainingSeries) {
        const QColor first(Qt::blue);
        const QColor later[] = {QColor(Qt::red), QColor(Qt::green)};
        GraphScene scene = oneSeriesScene();
        GraphSceneSeries series = scene.series.front();
        scene.series.clear();
        qreal level = 20.0;
        for (const QColor &colour : {later[0], later[1], first}) {
            series.color = colour;
            series.observations = {QPointF(0.0, level), QPointF(10.0, level)};
            scene.series.append(series);
            level += 30.0;
        }
        const QRectF bounds(0.0, 0.0, 300.0, 160.0);
        const auto paint = [&](const std::function<bool(const QImage &)> &cancelOnImage) {
            QImage image(bounds.size().toSize(), QImage::Format_RGB32);
            image.fill(kBackground);
            QPainter painter(&image);
            paintGraphScene(painter, scene, bounds, AxisPainter::Style{}, [&] { return cancelOnImage(image); });
            painter.end();
            return image;
        };

        const QImage whole = paint([](const QImage &) { return false; });
        const QImage cancelled = paint([&](const QImage &soFar) { return countPixelsAtColor(soFar, first) > 0; });

        ASSERT_GT(countPixelsAtColor(whole, first), 0);
        EXPECT_GT(countPixelsAtColor(cancelled, first), 0);
        for (const QColor &colour : later) {
            ASSERT_GT(countPixelsAtColor(whole, colour), 0) << colour.name().toStdString();
            EXPECT_EQ(countPixelsAtColor(cancelled, colour), 0) << colour.name().toStdString();
        }
    }

    TEST(CaptureRenderer, CancellingBetweenItemsStopsThereAndReturnsNoImage) {
        CaptureLayout layout = emptyLayout(QSizeF(300.0, 120.0));
        for (int row = 0; row < 5; ++row) {
            CaptureRectItem rect;
            rect.rect = QRectF(10.0, 10.0 + 20.0 * row, 280.0, 16.0);
            rect.fill = kForeground;
            layout.items.emplace_back(rect);
        }
        const QSize pixels = captureOutputPixels(layout.size, 100).value();
        const auto readsOf = [&](const int cancelOnRead) {
            int reads = 0;
            const std::optional<application::RasterImage> raster =
                    renderCapture(layout, pixels, [&] { return ++reads >= cancelOnRead; });
            return std::pair{reads, raster.has_value()};
        };

        const auto [wholeReads, wholeDelivered] = readsOf(INT_MAX);
        // Read 1 is before anything is painted and read 2 before the first item, so read 3 follows one item.
        const auto [cancelledReads, cancelledDelivered] = readsOf(3);

        ASSERT_TRUE(wholeDelivered);
        EXPECT_FALSE(cancelledDelivered);
        EXPECT_EQ(cancelledReads, 3);
        EXPECT_LT(cancelledReads, wholeReads);
    }

    TEST(CaptureLayoutItems, EqualityFollowsEveryItemField) {
        CaptureRectItem rect;
        rect.rect = QRectF(1.0, 2.0, 30.0, 40.0);
        rect.fill = kForeground;
        const CaptureTextItem text = textItem(QRectF(5.0, 6.0, 70.0, 20.0), QStringLiteral("Label"));
        const std::vector<CaptureItem> list{rect, text};
        const std::vector<CaptureItem> identical = list;

        CaptureTextItem otherText = text;
        otherText.text = QStringLiteral("Other");
        CaptureRectItem movedRect = rect;
        movedRect.rect.moveLeft(rect.rect.left() + 1.0);
        CaptureRectItem recoloured = rect;
        recoloured.fill = kBackground;

        EXPECT_TRUE(sameItems(list, identical));
        EXPECT_FALSE(sameItems(list, std::vector<CaptureItem>{rect, otherText}));
        EXPECT_FALSE(sameItems(list, std::vector<CaptureItem>{movedRect, text}));
        EXPECT_FALSE(sameItems(list, std::vector<CaptureItem>{recoloured, text}));
        EXPECT_FALSE(sameItems(list, std::vector<CaptureItem>{text, rect}));
    }
}
