#include "qt_data/raster_qimage.h"

#include <cstdint>
#include <memory>

namespace ksv::qt_data {
    QImage wrapRaster(const application::RasterImage &image) {
        using Storage = std::shared_ptr<const std::uint32_t[]>;
        auto *keepAlive = new Storage(image.storage());
        return QImage(reinterpret_cast<const uchar *>(image.pixels()), image.width(), image.height(),
                      static_cast<qsizetype>(image.stride()), QImage::Format_RGB32,
                      [](void *info) { delete static_cast<Storage *>(info); }, keepAlive);
    }
}
