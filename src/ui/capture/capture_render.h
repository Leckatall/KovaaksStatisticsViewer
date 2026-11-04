#pragma once

#include <QImage>
#include <QSize>
#include <QSizeF>
#include <QtGlobal>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <variant>

#include "app/contracts/raster_image.h"
#include "capture/capture_limits.h"
#include "capture/capture_snapshot.h"
#include "capture/graph_capture_layout.h"

namespace ksv::ui {
    using CaptureContent = std::variant<GraphCaptureSnapshot, TableCaptureSnapshot>;

    struct CaptureOptions {
        int percent = 200;
        bool includeSummary = true;
        // Empty means the reference size.
        std::optional<QSizeF> logical;
    };

    struct CaptureRenderRequest {
        quint64 session = 0;
        quint64 generation = 0;
        std::shared_ptr<const CaptureContent> content;
        CaptureOptions options;
        // Set by CaptureRenderQueue once the result is unwanted; a render stops at its next check.
        std::shared_ptr<const std::atomic_bool> cancelled;

        [[nodiscard]] bool isCancelled() const { return cancelled && cancelled->load(); }
    };

    struct CaptureRendered {
        application::RasterImage raster;
        // Bounded by CaptureLimits::thumbnailBytes, and shares no storage with `raster`.
        QImage thumbnail;
        QSize pixels;
        QSizeF logical;
    };

    struct CaptureRejected {
        CaptureRejection rejection;
        QSizeF logical;
    };

    struct CaptureRenderFailed {};

    using CaptureRenderOutcome = std::variant<CaptureRendered, CaptureRejected, CaptureRenderFailed>;
    using CaptureRenderFunction = std::function<CaptureRenderOutcome(const CaptureRenderRequest &)>;

    // The result touches no live object, so it may run on any thread.
    [[nodiscard]] CaptureRenderFunction makeCaptureRenderFunction(CaptureLimits limits, CaptureComposition composition);
}
