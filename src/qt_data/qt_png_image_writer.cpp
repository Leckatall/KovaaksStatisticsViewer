#include "qt_data/qt_png_image_writer.h"

#include <QImageWriter>
#include <QSaveFile>
#include <QString>

#include "qt_data/raster_qimage.h"

namespace ksv::qt_data {
    application::ImageOutputStatus QtPngImageWriter::write(const application::RasterImage &image,
                                                           const std::filesystem::path &destination) {
        using application::ImageOutputStatus;

        QSaveFile file(QString::fromStdWString(destination.wstring()));
        // A direct-write fallback would let a failed encode truncate an existing destination.
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) return ImageOutputStatus::DestinationUnavailable;

        if (!encode(image, file)) {
            file.cancelWriting();
            return ImageOutputStatus::EncodeFailed;
        }
        return file.commit() ? ImageOutputStatus::Succeeded : ImageOutputStatus::CommitFailed;
    }

    bool QtPngImageWriter::encode(const application::RasterImage &image, QIODevice &device) {
        QImageWriter writer(&device, "PNG");
        return writer.write(wrapRaster(image));
    }
}
