#pragma once

#include <functional>

#include <QImage>

#include "data/interfaces/i_clipboard_image_writer.h"

namespace ksv::qt_data {
    class QtClipboardImageWriter final : public data::IClipboardImageWriter {
    public:
        // The seam over the system clipboard; the default resolves it at call time.
        struct Backend {
            std::function<void(const QImage &)> publish;
            std::function<bool()> ownsClipboard;
        };

        explicit QtClipboardImageWriter(Backend backend = defaultBackend());

        application::ImageOutputStatus publish(const application::RasterImage &image) override;

        [[nodiscard]] static Backend defaultBackend();

    private:
        Backend m_backend;
    };
}
