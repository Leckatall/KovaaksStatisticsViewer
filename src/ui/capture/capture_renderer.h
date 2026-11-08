#pragma once

#include <QSize>

#include <optional>

#include "app/contracts/raster_image.h"
#include "capture/capture_layout.h"
#include "components/cancellation_check.h"

namespace ksv::ui {
    // Null is a caught render failure or a cancellation, checked between items and between graph series.
    // Touches no live object, so it may run on any thread.
    [[nodiscard]] std::optional<application::RasterImage> renderCapture(const CaptureLayout &layout, QSize pixels,
                                                                        const CancellationCheck &cancelled = {});
}
