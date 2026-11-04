#pragma once

#include <QSize>
#include <QSizeF>
#include <QtGlobal>

#include <optional>
#include <variant>

#include "capture/capture_layout.h"
#include "capture/capture_limits.h"
#include "capture/capture_snapshot.h"

namespace ksv::ui {
    struct CaptureComposition {
        QSizeF standardGraph;
        QSizeF minimumPlot;
    };

    [[nodiscard]] CaptureComposition standardCaptureComposition();

    [[nodiscard]] QSizeF graphReferenceSize(const GraphCaptureSnapshot &snapshot,
                                            const CaptureComposition &composition);

    [[nodiscard]] std::variant<CaptureLayout, CaptureRejection> layoutGraphCapture(
            const GraphCaptureSnapshot &snapshot, QSizeF logical, const CaptureComposition &composition);

    [[nodiscard]] qint64 captureSourcePoints(const GraphCaptureSnapshot &snapshot);

    [[nodiscard]] std::optional<CaptureRejection> suggestGraphResolution(
            const GraphCaptureSnapshot &snapshot, QSize pixels, int percent, const CaptureComposition &composition,
            const CaptureLimits &limits);
}
