#pragma once

#include <QSize>

#include <optional>

#include "app/contracts/raster_image.h"
#include "capture/capture_layout.h"

namespace ksv::ui {
    // Null is a caught render failure. Touches no live object, so it may run on any thread.
    [[nodiscard]] std::optional<application::RasterImage> renderCapture(const CaptureLayout &layout, QSize pixels);
}
