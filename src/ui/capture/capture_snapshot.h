#ifndef KOVAAKSSTATSVIEWER_CAPTURE_SNAPSHOT_H
#define KOVAAKSSTATSVIEWER_CAPTURE_SNAPSHOT_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <optional>
#include <vector>

#include "components/graph_scene.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/benchmarks/tracking/progress_grid_content.h"

namespace ksv::ui {
    // Every snapshot type is an owned value: no pointer, QObject or callable, so a snapshot outlives
    // the canvas and view models it was read from and can be laid out and painted off the GUI thread.

    enum class CaptureTarget {
        Run,
        DailyPlaytime,
        ScenarioHistory,
        BenchmarkRank,
        BenchmarkRecentRank,
        BenchmarkPlaytime,
        BenchmarkTable
    };

    [[nodiscard]] QString captureTargetName(CaptureTarget target);

    struct CaptureFontDescription {
        QStringList families;
        int pixelSize = 0;
    };

    struct CaptureAppearance {
        QColor window, base, text, button, buttonText, mid, placeholderText, highlight, attention;
        qreal groupTintAlpha = 0.0;
        CaptureFontDescription font;
    };

    struct GraphCaptureContext {
        QString title, graphType, rangeText, xTitle;
    };

    // `CaptureTarget::BenchmarkTable` never appears on a graph snapshot.
    struct GraphCaptureSnapshot {
        CaptureTarget target = CaptureTarget::Run;
        GraphCaptureContext context;
        GraphScene scene;
        bool keepEveryYLabel = false;
        CaptureAppearance appearance;
    };

    struct TableCaptureSummary {
        presentation::BenchmarkSummaryCaptions captions;
        QString attainedRank;
        QString attainedRankColor;
        QString completedRank;
        QString completedRankColor;
        QString averageRank;
        std::vector<double> averageRankFills;
        double completedRankPosition = 0.0;
        QString totalPlaytime;
        QString nextTier;
        QString nextTierColor;
        QString nextTierBlockerText;
    };

    struct TableCaptureSnapshot {
        QString benchmarkName;
        presentation::ProgressGridContent grid;
        std::optional<TableCaptureSummary> summary;
        QString completenessNote;
        CaptureAppearance appearance;
    };
}

#endif // KOVAAKSSTATSVIEWER_CAPTURE_SNAPSHOT_H
