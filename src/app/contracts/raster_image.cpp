#include "app/contracts/raster_image.h"

#include <limits>
#include <new>
#include <utility>

namespace ksv::application {
    std::optional<std::size_t> rasterByteSize(int width, int height) {
        if (width <= 0 || height <= 0) return std::nullopt;
        const std::uint64_t bytes = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4;
        if (bytes > std::numeric_limits<std::size_t>::max()) return std::nullopt;
        return static_cast<std::size_t>(bytes);
    }

    std::optional<RasterBuffer> RasterBuffer::allocate(int width, int height) {
        if (!rasterByteSize(width, height)) return std::nullopt;
        const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        try {
            return RasterBuffer(width, height, std::make_shared_for_overwrite<std::uint32_t[]>(pixelCount));
        } catch (const std::bad_alloc &) {
            return std::nullopt;
        }
    }

    RasterImage RasterBuffer::freeze() && {
        return RasterImage(m_width, m_height, std::move(m_pixels));
    }
}
