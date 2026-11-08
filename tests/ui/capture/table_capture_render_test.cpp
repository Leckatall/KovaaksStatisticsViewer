#include <gtest/gtest.h>

#include <QColor>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <variant>

#include "app/contracts/raster_image.h"
#include "capture/capture_layout.h"
#include "capture/capture_limits.h"
#include "capture/capture_renderer.h"
#include "capture/capture_sizing.h"
#include "capture/capture_snapshot.h"
#include "capture/table_capture_layout.h"
#include "table_capture_fixtures.h"

namespace ksv::ui {
    namespace {
        TableCaptureSnapshot wrappedNameTable() {
            QStringList words;
            for (int i = 0; i < 40; ++i) words.append(QStringLiteral("Wrapping%1").arg(i));
            const QStringList rowNames{words.join(QLatin1Char(' ')), QStringLiteral("Beside"), QStringLiteral("Lonely")};
            return tableFixtureSnapshot(
                rowNames, {tableFixtureGroup(QStringLiteral("Everything"), 0, 3, tableFixtureTint(0))}, 3);
        }

        QImage imageView(const application::RasterImage &raster) {
            return QImage(reinterpret_cast<const uchar *>(raster.pixels()), raster.width(), raster.height(),
                          static_cast<qsizetype>(raster.stride()), QImage::Format_RGB32);
        }

        std::optional<QRectF> largestRectFilled(const CaptureLayout &layout, const QColor &fill) {
            std::optional<QRectF> largest;
            for (const CaptureItem &item : layout.items) {
                const auto *rect = std::get_if<CaptureRectItem>(&item);
                if (!rect || rect->fill != fill) continue;
                if (!largest || rect->rect.width() * rect->rect.height() > largest->width() * largest->height()) {
                    largest = rect->rect;
                }
            }
            return largest;
        }

        qint64 pixelsAtColorWithin(const QImage &image, const QRect &region, const QColor &color) {
            const QRgb wanted = color.rgb();
            qint64 count = 0;
            for (int y = region.top(); y <= region.bottom(); ++y) {
                for (int x = region.left(); x <= region.right(); ++x) {
                    if (image.pixel(x, y) == wanted) ++count;
                }
            }
            return count;
        }

        QRect scaledRegion(const QRectF &logical, const qreal scale, const QImage &image) {
            return QRectF(logical.topLeft() * scale, logical.size() * scale)
                    .toAlignedRect()
                    .intersected(QRect(0, 0, image.width(), image.height()));
        }

        qint64 areaAt(const QSizeF logical, const int percent) {
            const QSize pixels = captureOutputPixels(logical, percent).value();
            return static_cast<qint64>(pixels.width()) * pixels.height();
        }

        CaptureLimits limitsWithArea(const qint64 maxPixelArea) {
            CaptureLimits limits;
            limits.maxEdgePixels = 1'000'000;
            limits.maxPixelArea = maxPixelArea;
            limits.maxEstimatedBytes = qint64{1} << 40;
            limits.maxSourcePoints = 1000;
            limits.thumbnailBytes = 0;
            return limits;
        }
    }

    TEST(TableCaptureRender, ResolutionChangesPixelsNotItems) {
        const TableCaptureSnapshot snapshot = wrappedNameTable();
        const CaptureLayout layout = layoutTableCapture(snapshot, false);

        const auto reference = captureOutputPixels(layout.size, 100);
        const auto doubled = captureOutputPixels(layout.size, 200);
        ASSERT_TRUE(reference.has_value());
        ASSERT_TRUE(doubled.has_value());
        const std::optional<application::RasterImage> at100 = renderCapture(layout, *reference);
        const std::optional<application::RasterImage> at200 = renderCapture(layout, *doubled);
        ASSERT_TRUE(at100.has_value());
        ASSERT_TRUE(at200.has_value());
        EXPECT_EQ(QSize(at100->width(), at100->height()), *reference);
        EXPECT_EQ(QSize(at200->width(), at200->height()), *doubled);

        EXPECT_TRUE(sameItems(layout.items, layoutTableCapture(snapshot, false).items));

        const QColor chunkFill(snapshot.grid.tiers.at(0).barColor);
        const std::optional<QRectF> chunk = largestRectFilled(layout, chunkFill);
        ASSERT_TRUE(chunk.has_value());
        for (const auto &[raster, scale] : {std::pair{&*at100, 1.0}, std::pair{&*at200, 2.0}}) {
            const QImage image = imageView(*raster);
            const QRect region = scaledRegion(*chunk, scale, image);
            ASSERT_FALSE(region.isEmpty());
            const qint64 area = static_cast<qint64>(region.width()) * region.height();
            EXPECT_GT(pixelsAtColorWithin(image, region, chunkFill) * 2, area) << "scale " << scale;
        }
    }

    TEST(TableCaptureRender, EveryResolutionIsAnExactMultipleOfTheLogicalSizeAtAnyFontSize) {
        for (int pixelSize = 9; pixelSize <= 30; ++pixelSize) {
            TableCaptureSnapshot snapshot = wrappedNameTable();
            snapshot.appearance.font.pixelSize = pixelSize;
            snapshot.summary = tableFixtureSummary(snapshot.grid.tiers);
            const CaptureLayout layout = layoutTableCapture(snapshot, true);

            const auto at100 = captureOutputPixels(layout.size, 100);
            const auto at200 = captureOutputPixels(layout.size, 200);
            ASSERT_TRUE(at100.has_value()) << "pixel size " << pixelSize;
            ASSERT_TRUE(at200.has_value()) << "pixel size " << pixelSize;
            EXPECT_EQ(*at200, *at100 * 2) << "pixel size " << pixelSize << ", logical " << layout.size.width() << "x"
                                          << layout.size.height();
        }
    }

    TEST(TableCaptureRender, AnOversizedTableAsksForALowerResolution) {
        const CaptureLayout layout = layoutTableCapture(wrappedNameTable(), false);
        const CaptureLimits limits = limitsWithArea(areaAt(layout.size, 100));
        ASSERT_GT(areaAt(layout.size, 200), limits.maxPixelArea);

        ASSERT_FALSE(validateCaptureOutput(CaptureSubject::Table, layout.size, 100, 0, limits).has_value());
        const auto rejection = validateCaptureOutput(CaptureSubject::Table, layout.size, 200, 0, limits);
        ASSERT_TRUE(rejection.has_value());
        EXPECT_EQ(rejection->correction, CaptureCorrection::ReduceResolution);
        ASSERT_TRUE(rejection->suggestedPercent.has_value());
        EXPECT_LT(*rejection->suggestedPercent, 200);
        EXPECT_FALSE(
            validateCaptureOutput(CaptureSubject::Table, layout.size, *rejection->suggestedPercent, 0, limits)
                .has_value());
    }

    TEST(TableCaptureRender, ATableUnsupportedAtTheMinimumSaysSo) {
        const TableCaptureSnapshot snapshot = wrappedNameTable();
        const CaptureLayout layout = layoutTableCapture(snapshot, false);
        const CaptureLimits limits = limitsWithArea(areaAt(layout.size, kMinimumCapturePercent) - 1);

        const auto rejection = validateCaptureOutput(CaptureSubject::Table, layout.size, 200, 0, limits);
        ASSERT_TRUE(rejection.has_value());
        EXPECT_EQ(rejection->correction, CaptureCorrection::UnsupportedAtMinimum);
        EXPECT_FALSE(rejection->suggestedPercent.has_value());

        const CaptureLayout again = layoutTableCapture(snapshot, false);
        EXPECT_TRUE(sameItems(layout.items, again.items));
        EXPECT_EQ(layout.size, again.size);
    }
}
