#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace ksv::application {
    class RasterImage;

    // Pixels are 0xffRRGGBB, tightly packed rows.
    [[nodiscard]] std::optional<std::size_t> rasterByteSize(int width, int height);

    // Sole writer of a raster; freeze() hands the storage to an immutable RasterImage.
    class RasterBuffer {
    public:
        RasterBuffer(const RasterBuffer &) = delete;
        RasterBuffer &operator=(const RasterBuffer &) = delete;
        RasterBuffer(RasterBuffer &&) noexcept = default;
        RasterBuffer &operator=(RasterBuffer &&) noexcept = default;

        [[nodiscard]] static std::optional<RasterBuffer> allocate(int width, int height);

        [[nodiscard]] int width() const { return m_width; }
        [[nodiscard]] int height() const { return m_height; }
        [[nodiscard]] std::uint32_t *pixels() { return m_pixels.get(); }

        [[nodiscard]] RasterImage freeze() &&;

    private:
        RasterBuffer(int width, int height, std::shared_ptr<std::uint32_t[]> pixels)
            : m_width(width), m_height(height), m_pixels(std::move(pixels)) {}

        int m_width;
        int m_height;
        std::shared_ptr<std::uint32_t[]> m_pixels;
    };

    // Immutable and cheap to copy: copies share one storage. There is no empty state.
    class RasterImage {
    public:
        [[nodiscard]] int width() const { return m_width; }
        [[nodiscard]] int height() const { return m_height; }
        [[nodiscard]] std::size_t stride() const { return static_cast<std::size_t>(m_width) * 4; }
        [[nodiscard]] std::size_t byteSize() const { return stride() * static_cast<std::size_t>(m_height); }
        [[nodiscard]] const std::uint32_t *pixels() const { return m_pixels.get(); }
        [[nodiscard]] std::shared_ptr<const std::uint32_t[]> storage() const { return m_pixels; }

    private:
        friend class RasterBuffer;
        RasterImage(int width, int height, std::shared_ptr<const std::uint32_t[]> pixels)
            : m_width(width), m_height(height), m_pixels(std::move(pixels)) {}

        int m_width;
        int m_height;
        std::shared_ptr<const std::uint32_t[]> m_pixels;
    };
}
