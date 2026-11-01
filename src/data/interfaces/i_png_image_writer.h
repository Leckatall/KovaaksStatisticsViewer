#pragma once

#include <filesystem>

#include "app/contracts/image_output.h"
#include "app/contracts/raster_image.h"

namespace ksv::data {
    class IPngImageWriter {
    public:
        virtual ~IPngImageWriter() = default;

        // Called on the save worker thread.
        virtual application::ImageOutputStatus write(const application::RasterImage &image,
                                                     const std::filesystem::path &destination) = 0;
    };
}
