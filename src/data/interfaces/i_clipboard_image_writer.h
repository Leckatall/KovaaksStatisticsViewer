#pragma once

#include "app/contracts/image_output.h"
#include "app/contracts/raster_image.h"

namespace ksv::data {
    class IClipboardImageWriter {
    public:
        virtual ~IClipboardImageWriter() = default;

        // Called on the GUI thread.
        virtual application::ImageOutputStatus publish(const application::RasterImage &image) = 0;
    };
}
