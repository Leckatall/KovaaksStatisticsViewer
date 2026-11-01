#include <cstdint>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include <QColor>
#include <QImage>

#include "app/contracts/raster_image.h"
#include "qt_data/raster_qimage.h"

namespace ksv::qt_data {
    namespace {
        constexpr std::uint32_t kPixels[6] = {0xffff0000u, 0xff00ff00u, 0xff0000ffu,
                                              0xff102030u, 0xff405060u, 0xff708090u};

        std::optional<application::RasterImage> knownImage() {
            auto buffer = application::RasterBuffer::allocate(3, 2);
            if (!buffer) return std::nullopt;
            for (int i = 0; i < 6; ++i) buffer->pixels()[i] = kPixels[i];
            return std::move(*buffer).freeze();
        }
    }

    TEST(RasterQImage, WrapsTheSameStorage) {
        const auto raster = knownImage();
        ASSERT_TRUE(raster);

        const QImage image = wrapRaster(*raster);

        ASSERT_FALSE(image.isNull());
        EXPECT_EQ(image.format(), QImage::Format_RGB32);
        EXPECT_EQ(image.width(), 3);
        EXPECT_EQ(image.height(), 2);
        EXPECT_EQ(image.bytesPerLine(), 12);
        EXPECT_FALSE(image.hasAlphaChannel());
        EXPECT_EQ(reinterpret_cast<const std::uint32_t *>(image.constBits()), raster->pixels());
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 3; ++x)
                EXPECT_EQ(image.pixelColor(x, y), QColor::fromRgb(kPixels[y * 3 + x]));
    }

    TEST(RasterQImage, OutlivesTheOperationThatMadeIt) {
        auto raster = knownImage();
        ASSERT_TRUE(raster);
        std::weak_ptr<const std::uint32_t[]> storage = raster->storage();
        std::optional<QImage> image = wrapRaster(*raster);
        raster.reset();

        ASSERT_FALSE(image->isNull());
        EXPECT_FALSE(storage.expired());
        EXPECT_EQ(image->pixelColor(2, 1), QColor::fromRgb(kPixels[5]));

        image.reset();
        EXPECT_TRUE(storage.expired());
    }
}
