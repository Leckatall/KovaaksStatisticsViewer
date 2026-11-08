#pragma once

#include <filesystem>

#include "data/interfaces/i_clipboard_image_writer.h"
#include "data/interfaces/i_png_image_writer.h"

namespace ksv::gallery {
    // Dev tools must never reach the user's clipboard or disk, so these report failure rather than succeed silently.
    class InertClipboardImageWriter final : public data::IClipboardImageWriter {
    public:
        application::ImageOutputStatus publish(const application::RasterImage &) override {
            return application::ImageOutputStatus::ClipboardNotConfirmed;
        }
    };

    class InertPngImageWriter final : public data::IPngImageWriter {
    public:
        application::ImageOutputStatus write(const application::RasterImage &,
                                             const std::filesystem::path &) override {
            return application::ImageOutputStatus::DestinationUnavailable;
        }
    };
}
