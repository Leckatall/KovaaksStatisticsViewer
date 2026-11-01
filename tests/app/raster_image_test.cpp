#include <climits>
#include <cstdint>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "app/contracts/raster_image.h"

namespace ksv::application {
    namespace {
        std::optional<RasterImage> frozenImage(int width, int height) {
            auto buffer = RasterBuffer::allocate(width, height);
            if (!buffer) return std::nullopt;
            for (int i = 0; i < width * height; ++i) buffer->pixels()[i] = 0xff000000u | static_cast<std::uint32_t>(i);
            return std::move(*buffer).freeze();
        }
    }

    TEST(RasterImage, StrideIsWidthTimesFour) {
        auto buffer = RasterBuffer::allocate(3, 2);
        ASSERT_TRUE(buffer);
        for (std::uint32_t i = 0; i < 6; ++i) buffer->pixels()[i] = 0xff000000u | (i * 17);

        const RasterImage image = std::move(*buffer).freeze();

        EXPECT_EQ(image.width(), 3);
        EXPECT_EQ(image.height(), 2);
        EXPECT_EQ(image.stride(), 12u);
        EXPECT_EQ(image.byteSize(), 24u);
        for (std::uint32_t i = 0; i < 6; ++i) EXPECT_EQ(image.pixels()[i], 0xff000000u | (i * 17));
    }

    TEST(RasterImage, ACopySharesStorage) {
        std::optional<RasterImage> original = frozenImage(3, 2);
        ASSERT_TRUE(original);
        const std::uint32_t *pixels = original->pixels();
        const std::uint32_t first = pixels[0];
        const std::uint32_t last = pixels[5];
        std::weak_ptr<const std::uint32_t[]> weak = original->storage();

        std::optional<RasterImage> copy = *original;
        original.reset();

        EXPECT_EQ(copy->pixels(), pixels);
        EXPECT_EQ(copy->pixels()[0], first);
        EXPECT_EQ(copy->pixels()[5], last);
        EXPECT_FALSE(weak.expired());

        copy.reset();
        EXPECT_TRUE(weak.expired());
    }

    TEST(RasterImage, AnUnsatisfiableRequestIsNotAnEmptyImage) {
        if constexpr (sizeof(std::size_t) == 8) {
            EXPECT_FALSE(RasterBuffer::allocate(INT_MAX, INT_MAX));
        }
    }

    TEST(RasterImage, ByteSizeIsCheckedArithmetic) {
        ASSERT_TRUE(rasterByteSize(1, 1));
        EXPECT_EQ(*rasterByteSize(1, 1), 4u);

        const auto wide = rasterByteSize(INT_MAX, 1);
        ASSERT_TRUE(wide);
        EXPECT_EQ(*wide, static_cast<std::size_t>(INT_MAX) * 4u);

        if constexpr (sizeof(std::size_t) == 8) {
            const auto huge = rasterByteSize(INT_MAX, INT_MAX);
            ASSERT_TRUE(huge);
            EXPECT_EQ(*huge, static_cast<std::uint64_t>(INT_MAX) * static_cast<std::uint64_t>(INT_MAX) * 4u);
        }

        EXPECT_FALSE(rasterByteSize(0, 4));
        EXPECT_FALSE(rasterByteSize(4, 0));
        EXPECT_FALSE(rasterByteSize(-1, 4));
    }
}
