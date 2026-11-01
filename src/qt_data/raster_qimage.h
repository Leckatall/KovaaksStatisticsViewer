#pragma once

#include <QImage>

#include "app/contracts/raster_image.h"

namespace ksv::qt_data {
    // A Format_RGB32 image over the raster's own storage, which it keeps alive.
    [[nodiscard]] QImage wrapRaster(const application::RasterImage &image);
}
