#pragma once

#include "capture/capture_layout.h"
#include "capture/capture_snapshot.h"

namespace ksv::ui {
    [[nodiscard]] CaptureLayout layoutTableCapture(const TableCaptureSnapshot &snapshot, bool includeSummary);
}
