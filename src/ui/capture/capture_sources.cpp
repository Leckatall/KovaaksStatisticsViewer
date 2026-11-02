#include "capture/capture_sources.h"

#include <QDateTime>
#include <QObject>
#include <QTimeZone>
#include <QtAssert>

#include <algorithm>

namespace ksv::ui {
    namespace {
        // The range the axis was given, not its resolved bounds, so a selection past the last observation shows.
        QString calendarIntervalText(const presentation::Axis &axis) {
            const auto range = axis.calendarRangeMs();
            if (!range) return {};
            const auto day = [](const qint64 ms) {
                return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc()).date().toString(Qt::ISODate);
            };
            const QString first = day(range->first);
            const QString last = day(range->second);
            return first == last ? first : QObject::tr("%1 to %2").arg(first, last);
        }

        bool hasDrawableContent(const GraphScene &scene) {
            const bool carriesCount = scene.connection != SeriesPainter::Connection::None;
            return std::ranges::any_of(scene.series, [carriesCount](const GraphSceneSeries &series) {
                return !series.observations.isEmpty() ||
                       (carriesCount && (series.leadingCarry || series.trailingCarry));
            });
        }
    }

    std::optional<GraphCaptureSnapshot> captureGraph(const CaptureTarget target, const GraphCanvas &canvas,
                                                     GraphCaptureContext context,
                                                     const CaptureAppearance &appearance) {
        Q_ASSERT(target != CaptureTarget::BenchmarkTable);
        if (target == CaptureTarget::BenchmarkTable || !canvas.graphVm()) return std::nullopt;

        GraphScene scene = canvas.scene();
        scene.selectedBand.reset();
        if (!hasDrawableContent(scene)) return std::nullopt;

        GraphCaptureSnapshot snapshot;
        snapshot.target = target;
        snapshot.context = std::move(context);
        snapshot.scene = std::move(scene);
        snapshot.keepEveryYLabel = target == CaptureTarget::BenchmarkRank ||
                                   target == CaptureTarget::BenchmarkRecentRank;
        snapshot.appearance = appearance;
        return snapshot;
    }

    std::optional<TableCaptureSnapshot> captureTable(const presentation::BenchmarkTrackingViewModel &vm,
                                                     const CaptureAppearance &appearance) {
        const bool incomplete = vm.state() == QLatin1String("incomplete");
        if (!incomplete && vm.state() != QLatin1String("trackable")) return std::nullopt;
        if (!vm.progressGrid()) return std::nullopt;

        TableCaptureSnapshot snapshot;
        snapshot.grid = vm.progressGrid()->content();
        if (snapshot.grid.rows.empty()) return std::nullopt;
        snapshot.benchmarkName = vm.selectedName();
        snapshot.appearance = appearance;
        if (incomplete) snapshot.completenessNote = QObject::tr("Not ready to rank: the benchmark definition is incomplete.");

        if (vm.summaryAvailable()) {
            TableCaptureSummary summary;
            summary.attainedRank = vm.attainedRank();
            summary.attainedRankColor = vm.attainedRankColor();
            summary.completedRank = vm.completedRank();
            summary.completedRankColor = vm.completedRankColor();
            summary.averageRank = vm.averageRank();
            for (const QVariant &fill: vm.averageRankFills()) summary.averageRankFills.push_back(fill.toDouble());
            summary.completedRankPosition = vm.completedRankPosition();
            summary.totalPlaytime = vm.totalPlaytime();
            summary.nextTier = vm.nextTier();
            summary.nextTierColor = vm.nextTierColor();
            summary.nextTierBlockerText = vm.nextTierBlockerText();
            snapshot.summary = std::move(summary);
        }
        return snapshot;
    }

    GraphCaptureContext runCaptureContext(const presentation::GraphViewModel &vm) {
        // The run label already carries the date, so there is no range text.
        return {.title = vm.scenarioTitle(),
                .graphType = QObject::tr("Run performance"),
                .rangeText = {},
                .xTitle = QObject::tr("Elapsed time (seconds)")};
    }

    GraphCaptureContext dailyPlaytimeCaptureContext(const presentation::PlaytimeGraphViewModel &vm) {
        return {.title = vm.title(),
                .graphType = QObject::tr("Daily playtime"),
                .rangeText = calendarIntervalText(vm.xAxis()),
                .xTitle = QObject::tr("Date")};
    }

    GraphCaptureContext scenarioHistoryCaptureContext(const presentation::CompletionHistoryViewModel &vm) {
        QString range;
        const auto span = vm.visibleRunSpan();
        const auto times = vm.visibleStartTimesMs();
        if (span && times) {
            range = QObject::tr("Runs #%1 to #%2 (%3 to %4)")
                    .arg(span->first).arg(span->second)
                    .arg(QDateTime::fromMSecsSinceEpoch(times->first).date().toString(Qt::ISODate),
                         QDateTime::fromMSecsSinceEpoch(times->second).date().toString(Qt::ISODate));
        }
        return {.title = vm.scenarioTitle(),
                .graphType = QObject::tr("Scenario history"),
                .rangeText = range,
                .xTitle = QObject::tr("Run (runs counted in order)")};
    }

    GraphCaptureContext benchmarkHistoryCaptureContext(const QString &benchmarkName,
                                                       const presentation::BenchmarkHistoryViewModel &vm) {
        return {.title = benchmarkName,
                .graphType = vm.metricName(),
                .rangeText = calendarIntervalText(vm.xAxis()),
                .xTitle = QObject::tr("Date")};
    }
}
