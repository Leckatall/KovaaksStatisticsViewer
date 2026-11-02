#ifndef KOVAAKSSTATSVIEWER_CAPTURE_SOURCES_H
#define KOVAAKSSTATSVIEWER_CAPTURE_SOURCES_H

#include <optional>

#include "capture/capture_snapshot.h"
#include "components/graph_canvas.h"
#include "presentation/benchmarks/tracking/benchmark_history_vm.h"
#include "presentation/graphs/completion_history_vm.h"
#include "presentation/graphs/graph_vm.h"
#include "presentation/graphs/playtime_graph_vm.h"

namespace ksv::ui {
    // Null means the graph has nothing drawable to capture.
    [[nodiscard]] std::optional<GraphCaptureSnapshot> captureGraph(CaptureTarget target, const GraphCanvas &canvas,
                                                                   GraphCaptureContext context,
                                                                   const CaptureAppearance &appearance);

    // Null means there is no usable table; an incomplete benchmark is captured without a summary.
    [[nodiscard]] std::optional<TableCaptureSnapshot> captureTable(const presentation::BenchmarkTrackingViewModel &vm,
                                                                   const CaptureAppearance &appearance);

    [[nodiscard]] GraphCaptureContext runCaptureContext(const presentation::GraphViewModel &vm);
    [[nodiscard]] GraphCaptureContext dailyPlaytimeCaptureContext(const presentation::PlaytimeGraphViewModel &vm);
    [[nodiscard]] GraphCaptureContext scenarioHistoryCaptureContext(
        const presentation::CompletionHistoryViewModel &vm);
    [[nodiscard]] GraphCaptureContext benchmarkHistoryCaptureContext(
        const QString &benchmarkName, const presentation::BenchmarkHistoryViewModel &vm);
}

#endif // KOVAAKSSTATSVIEWER_CAPTURE_SOURCES_H
