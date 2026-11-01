#include "qt_data/qt_clipboard_image_writer.h"

#include <utility>

#include <QClipboard>
#include <QGuiApplication>

#include "qt_data/raster_qimage.h"

namespace ksv::qt_data {
    QtClipboardImageWriter::QtClipboardImageWriter(Backend backend) : m_backend(std::move(backend)) {}

    application::ImageOutputStatus QtClipboardImageWriter::publish(const application::RasterImage &image) {
        m_backend.publish(wrapRaster(image));
        return m_backend.ownsClipboard() ? application::ImageOutputStatus::Succeeded
                                         : application::ImageOutputStatus::ClipboardNotConfirmed;
    }

    QtClipboardImageWriter::Backend QtClipboardImageWriter::defaultBackend() {
        return {[](const QImage &image) { QGuiApplication::clipboard()->setImage(image); },
                [] { return QGuiApplication::clipboard()->ownsClipboard(); }};
    }
}
