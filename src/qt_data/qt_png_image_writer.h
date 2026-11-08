#pragma once

#include <QIODevice>

#include "data/interfaces/i_png_image_writer.h"

namespace ksv::qt_data {
    class QtPngImageWriter final : public data::IPngImageWriter {
    public:
        application::ImageOutputStatus write(const application::RasterImage &image,
                                             const std::filesystem::path &destination) override;

        [[nodiscard]] static bool encode(const application::RasterImage &image, QIODevice &device);
    };
}
