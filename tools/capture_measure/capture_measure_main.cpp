// Measures capture cost for the validation gate. One CSV row per case; `--all` respawns this executable once per
// case so the peak working set, a process-lifetime high-water mark, belongs to that case alone.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "capture/capture_appearance.h"
#include "capture/capture_render.h"
#include "capture/capture_sizing.h"
#include "capture/capture_sources.h"
#include "capture/graph_capture_layout.h"
#include "capture/table_capture_layout.h"
#include "components/graph_canvas.h"
#include "presentation/graphs/common/axes/value_axis.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"
#include "qt_data/qt_clipboard_image_writer.h"
#include "qt_data/qt_png_image_writer.h"
#include "table_capture_fixtures.h"

namespace ksv::ui {
    namespace {
        using presentation::SeriesModel;
        using presentation::ValueAxis;
        using Clock = std::chrono::steady_clock;

        constexpr auto kThemeUri = "KsvCaptureMeasure";
        constexpr int kTableTiers = 8;
        constexpr int kGraphXMax = 3600;

        struct Env {
            CaptureAppearance appearance;
            CaptureLimits active;
            CaptureLimits provisional;
            CaptureComposition composition;
            QString outDir;
        };

        using ContentPtr = std::shared_ptr<const CaptureContent>;

        struct Case {
            QString name;
            QString scale;
            // Zero marks a snapshot-creation case, which renders nothing.
            int percent = 0;
            std::function<ContentPtr(const Env &)> build;
            std::function<double(const Env &)> snapshotMs;
        };

        struct Row {
            QString name;
            QString scale;
            int percent = 0;
            int width = -1;
            int height = -1;
            qint64 peakWorkingSet = -1;
            double snapshotMs = -1;
            double layoutPaintMs = -1;
            double pngMs = -1;
            double longestGapMs = -1;
            QString status = QStringLiteral("failed");
            qint64 pngBytes = -1;
        };

        double msSince(const Clock::time_point start) {
            return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        }

        QString num(const double v) { return v < 0 ? QString() : QString::number(v, 'f', 2); }
        QString inum(const qint64 v) { return v < 0 ? QString() : QString::number(v); }

        QString csvHeader() {
            return QStringLiteral(
                "case,input_scale,percent,width,height,peak_working_set_bytes,snapshot_ms,layout_paint_ms,"
                "png_write_ms,longest_gui_gap_ms,status,png_bytes");
        }

        QString csvRow(const Row &r) {
            return QStringList{r.name, r.scale, r.percent > 0 ? QString::number(r.percent) : QString(),
                               inum(r.width), inum(r.height), inum(r.peakWorkingSet), num(r.snapshotMs),
                               num(r.layoutPaintMs), num(r.pngMs), num(r.longestGapMs), r.status, inum(r.pngBytes)}
                    .join(QLatin1Char(','));
        }

        qint64 peakWorkingSet() {
            PROCESS_MEMORY_COUNTERS counters{};
            counters.cb = sizeof(counters);
            if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) return -1;
            return static_cast<qint64>(counters.PeakWorkingSetSize);
        }

        QString rejectionName(const CaptureRejectionReason reason) {
            switch (reason) {
                case CaptureRejectionReason::TooLittleRoom: return QStringLiteral("TooLittleRoom");
                case CaptureRejectionReason::EdgeLimit: return QStringLiteral("EdgeLimit");
                case CaptureRejectionReason::AreaLimit: return QStringLiteral("AreaLimit");
                case CaptureRejectionReason::MemoryLimit: return QStringLiteral("MemoryLimit");
                case CaptureRejectionReason::SourceGeometry: return QStringLiteral("SourceGeometry");
                case CaptureRejectionReason::ArithmeticOverflow: return QStringLiteral("ArithmeticOverflow");
            }
            return QStringLiteral("Unknown");
        }

        // ---- graph sources -------------------------------------------------------------------------------

        class MeasureVm : public presentation::GraphViewModelBase {
        public:
            MeasureVm() { m_xAxis.setRange(0.0, kGraphXMax); }

            SeriesModel &addSeries(const int column, const QString &name) {
                auto *series = new SeriesModel(this);
                series->setColumn(column);
                series->setId(QStringLiteral("id-%1").arg(column));
                series->setName(name);
                static const QColor colors[] = {QColor("#42d9e8"), QColor("#e67828"), QColor("#8fd45a"),
                                                QColor("#d96bd0"), QColor("#e8d242"), QColor("#6b8fe6"),
                                                QColor("#e66b6b"), QColor("#b0b0b0")};
                series->setColor(colors[(column - 1) % 8]);
                m_series.append(series);
                return *series;
            }

            [[nodiscard]] QList<SeriesModel *> series(const QList<int> &columns) const override {
                QList<SeriesModel *> result;
                for (auto *series: m_series) {
                    if (columns.contains(series->column())) result.append(series);
                }
                return result;
            }

            [[nodiscard]] const presentation::Axis &xAxis() const override { return m_xAxis; }
            [[nodiscard]] int yAxisColumn() const override { return 1; }

        private:
            ValueAxis m_xAxis;
            QList<SeriesModel *> m_series;
        };

        QList<QPointF> syntheticPoints(const int seriesIndex, const int count) {
            std::mt19937 rng(static_cast<std::mt19937::result_type>(1234 + seriesIndex));
            std::normal_distribution<double> noise(0.0, 4.0);
            QList<QPointF> points;
            points.reserve(count);
            for (int i = 0; i < count; ++i) {
                const double t = count > 1 ? static_cast<double>(i) / (count - 1) : 0.0;
                const double y = 50.0 + 8.0 * seriesIndex + 25.0 * std::sin(t * 40.0 + seriesIndex) + noise(rng);
                points.append(QPointF(t * kGraphXMax, std::clamp(y, 0.0, 100.0)));
            }
            return points;
        }

        QList<QPointF> endpoints() { return {QPointF(0.0, 0.0), QPointF(kGraphXMax, 100.0)}; }

        std::unique_ptr<GraphCanvas> makeCanvas(presentation::GraphViewModelBase &vm, const int seriesCount) {
            auto canvas = std::make_unique<GraphCanvas>();
            canvas->setWidth(800);
            canvas->setHeight(400);
            canvas->setGraphVm(&vm);
            QVariantList columns;
            for (int s = 1; s <= seriesCount; ++s) columns.append(s);
            canvas->setVisibleColumns(columns);
            canvas->setConnectionMode(GraphCanvas::ConnectionMode::Straight);
            return canvas;
        }

        GraphCaptureContext denseContext() {
            return GraphCaptureContext{.title = QStringLiteral("Dense measurement run"),
                                       .graphType = QStringLiteral("Run graph"),
                                       .rangeText = QStringLiteral("0 to 3600 s"),
                                       .xTitle = QStringLiteral("Time (s)")};
        }

        QString seriesName(const int s, const bool longLabel) {
            if (!longLabel) return QStringLiteral("Series %1").arg(s + 1);
            return QStringLiteral(
                       "Series %1: a deliberately long scenario label describing the variant, the sensitivity "
                       "profile and the aim style so that the legend has to wrap or truncate it")
                    .arg(s + 1);
        }

        // The scene structure comes from a two-point canvas, then the observations are swapped for the full
        // synthetic data, so a render case does not carry the live view model's copy of the points.
        ContentPtr denseContent(const Env &env, const int seriesCount, const int points, const bool longLabel) {
            MeasureVm vm;
            for (int s = 0; s < seriesCount; ++s) {
                vm.addSeries(s + 1, seriesName(s, longLabel)).setData(endpoints());
            }
            const auto canvas = makeCanvas(vm, seriesCount);
            GraphCaptureContext context = denseContext();
            if (longLabel) {
                context.title = QStringLiteral(
                    "A very long graph title that names the scenario, the benchmark, the date range and the "
                    "player so that the heading has to cope with real world lengths");
            }
            auto snapshot = captureGraph(CaptureTarget::Run, *canvas, context, env.appearance);
            if (!snapshot) return nullptr;
            for (int s = 0; s < snapshot->scene.series.size(); ++s) {
                snapshot->scene.series[s].observations = syntheticPoints(s, points);
            }
            return std::make_shared<const CaptureContent>(*std::move(snapshot));
        }

        double denseSnapshotMs(const Env &env, const int seriesCount, const int points) {
            MeasureVm vm;
            for (int s = 0; s < seriesCount; ++s) {
                vm.addSeries(s + 1, seriesName(s, false)).setData(syntheticPoints(s, points));
            }
            const auto canvas = makeCanvas(vm, seriesCount);
            const auto start = Clock::now();
            const auto snapshot = captureGraph(CaptureTarget::Run, *canvas, denseContext(), env.appearance);
            const double ms = msSince(start);
            return snapshot ? ms : -1.0;
        }

        // ---- table sources -------------------------------------------------------------------------------

        std::vector<presentation::ProgressGridTier> measureTiers(const int count) {
            std::vector<presentation::ProgressGridTier> tiers;
            for (int i = 0; i < count; ++i) {
                presentation::ProgressGridTier tier;
                tier.id = QStringLiteral("tier-%1").arg(i + 1);
                tier.name = QStringLiteral("Tier %1").arg(i + 1);
                tier.color = QColor::fromHsv((i * 43) % 360, 120, 245).name();
                tier.barColor = QColor::fromHsv((i * 43) % 360, 230, 200).name();
                tiers.push_back(tier);
            }
            return tiers;
        }

        TableCaptureSnapshot buildTable(const Env &env, const int rows, const int tierCount) {
            TableCaptureSnapshot snapshot;
            snapshot.benchmarkName = QStringLiteral("Measurement Benchmark");
            snapshot.appearance = env.appearance;
            snapshot.grid.tiers = measureTiers(tierCount);
            for (int i = 0; i < rows; ++i) {
                snapshot.grid.rows.push_back(
                    tableFixtureRow(QStringLiteral("Measurement Drill %1").arg(i), i, snapshot.grid.tiers));
            }
            for (int start = 0, group = 0; start < rows; start += 8, ++group) {
                snapshot.grid.groups.push_back(tableFixtureGroup(QStringLiteral("Category %1").arg(group), start,
                                                                 std::min(8, rows - start), tableFixtureTint(group)));
            }
            snapshot.summary = tableFixtureSummary(snapshot.grid.tiers);
            return snapshot;
        }

        ContentPtr overflowTable(const Env &env) {
            TableCaptureSnapshot snapshot = largeTableFixture();
            snapshot.appearance = env.appearance;
            snapshot.summary = tableFixtureSummary(snapshot.grid.tiers);
            return std::make_shared<const CaptureContent>(std::move(snapshot));
        }

        enum class Metric { Edge, Area, Bytes };

        bool withinFraction(const Metric metric, const QSizeF logical, const int percent, const double fraction,
                            const CaptureLimits &limits) {
            const std::optional<QSize> pixels = captureOutputPixels(logical, percent);
            if (!pixels) return false;
            switch (metric) {
                case Metric::Edge:
                    return std::max(pixels->width(), pixels->height()) <= fraction * limits.maxEdgePixels;
                case Metric::Area:
                    return static_cast<double>(pixels->width()) * pixels->height() <= fraction * limits.maxPixelArea;
                case Metric::Bytes: {
                    const auto bytes = estimateCaptureBytes(pixels, 0, limits);
                    return bytes && static_cast<double>(*bytes) <= fraction * limits.maxEstimatedBytes;
                }
            }
            return false;
        }

        // Height is linear in the row count (checked at 40 and 80 rows), so the largest table under a
        // fraction of one provisional limit is found by search over the model rather than by laying out each.
        ContentPtr nearLimitTable(const Env &env, const Metric metric, const int percent, const double fraction) {
            const QSizeF small = layoutTableCapture(buildTable(env, 40, kTableTiers), true).size;
            const QSizeF large = layoutTableCapture(buildTable(env, 80, kTableTiers), true).size;
            const double slope = (large.height() - small.height()) / 40.0;
            if (slope <= 0.0) return nullptr;
            const auto fits = [&](const int rows) {
                const QSizeF logical(large.width(), small.height() + slope * (rows - 40));
                return withinFraction(metric, logical, percent, fraction, env.provisional);
            };
            int lo = 1, hi = 1'000'000;
            while (lo < hi) {
                const int mid = lo + (hi - lo + 1) / 2;
                if (fits(mid)) lo = mid;
                else hi = mid - 1;
            }
            return std::make_shared<const CaptureContent>(buildTable(env, lo, kTableTiers));
        }

        // ---- case list -----------------------------------------------------------------------------------

        std::vector<Case> allCases() {
            std::vector<Case> cases;
            const auto add = [&](QString name, QString scale, const int percent,
                                 std::function<ContentPtr(const Env &)> build) {
                cases.push_back(Case{std::move(name), std::move(scale), percent, std::move(build), {}});
            };

            for (const auto [series, points]: {std::pair{4, 25'000}, std::pair{8, 100'000}}) {
                const QString scale = QStringLiteral("%1x%2").arg(series).arg(points);
                for (const int percent: {100, 200, 500}) {
                    add(QStringLiteral("dense-%1-%2").arg(scale).arg(percent), scale, percent,
                        [=](const Env &env) { return denseContent(env, series, points, false); });
                }
            }
            for (const auto [series, points]: {std::pair{4, 25'000}, std::pair{8, 100'000}}) {
                const QString scale = QStringLiteral("%1x%2").arg(series).arg(points);
                cases.push_back(Case{QStringLiteral("snapshot-%1").arg(scale), scale, 0, {},
                                     [=](const Env &env) { return denseSnapshotMs(env, series, points); }});
            }
            add(QStringLiteral("dense-near-points-200"), QStringLiteral("8x237500"), 200,
                [](const Env &env) { return denseContent(env, 8, 237'500, false); });

            for (const int percent: {100, 200}) {
                add(QStringLiteral("long-labels-%1").arg(percent), QStringLiteral("6x500"), percent,
                    [](const Env &env) { return denseContent(env, 6, 500, true); });
            }
            for (const int percent: {100, 200, 500}) {
                add(QStringLiteral("overflow-table-%1").arg(percent), QStringLiteral("40rows-8tiers"), percent,
                    [](const Env &env) { return overflowTable(env); });
            }
            add(QStringLiteral("many-tier-table-200"), QStringLiteral("40rows-20tiers"), 200, [](const Env &env) {
                return std::make_shared<const CaptureContent>(buildTable(env, 40, 20));
            });

            const struct {
                const char *name;
                Metric metric;
                double fraction;
            } limitCases[] = {{"table-near-edge-500", Metric::Edge, 0.95},
                        {"table-near-area-500", Metric::Area, 0.95},
                        {"table-near-bytes-500", Metric::Bytes, 0.95},
                        {"save-largest-500", Metric::Area, 0.995}};
            for (const auto &entry: limitCases) {
                const Metric metric = entry.metric;
                const double fraction = entry.fraction;
                add(QString::fromLatin1(entry.name), QStringLiteral("%1pct-of-limit").arg(fraction * 100.0), 500,
                    [=](const Env &env) { return nearLimitTable(env, metric, 500, fraction); });
            }
            return cases;
        }

        // ---- running a case -------------------------------------------------------------------------------

        Row runSnapshotCase(const Case &c, const Env &env) {
            Row row{.name = c.name, .scale = c.scale};
            row.snapshotMs = c.snapshotMs(env);
            row.status = row.snapshotMs >= 0 ? QStringLiteral("rendered") : QStringLiteral("failed");
            row.peakWorkingSet = peakWorkingSet();
            return row;
        }

        // The render and the PNG write both run on the worker; the gap is the longest wait between 5 ms timer
        // ticks on the GUI thread across both.
        Row runRenderCase(const Case &c, const Env &env) {
            Row row{.name = c.name, .scale = c.scale, .percent = c.percent};
            const ContentPtr content = c.build(env);
            if (!content) {
                row.peakWorkingSet = peakWorkingSet();
                return row;
            }

            const CaptureRenderFunction render = makeCaptureRenderFunction(env.active, env.composition);
            CaptureRenderRequest request;
            request.content = content;
            request.options.percent = c.percent;
            const QString pngPath = QDir(env.outDir).absoluteFilePath(c.name + QStringLiteral(".png"));

            std::optional<CaptureRenderOutcome> outcome;
            application::ImageOutputStatus pngStatus = application::ImageOutputStatus::Unexpected;
            bool wrote = false;

            QEventLoop loop;
            QTimer timer;
            timer.setTimerType(Qt::PreciseTimer);
            timer.setInterval(5);
            Clock::time_point last = Clock::now();
            double maxGap = 0.0;
            QObject::connect(&timer, &QTimer::timeout, [&] {
                const auto now = Clock::now();
                maxGap = std::max(maxGap, std::chrono::duration<double, std::milli>(now - last).count());
                last = now;
            });

            std::thread worker([&] {
                const auto renderStart = Clock::now();
                outcome = render(request);
                row.layoutPaintMs = msSince(renderStart);
                if (const auto *rendered = std::get_if<CaptureRendered>(&*outcome)) {
                    qt_data::QtPngImageWriter writer;
                    const auto writeStart = Clock::now();
                    pngStatus = writer.write(rendered->raster, std::filesystem::path(pngPath.toStdWString()));
                    row.pngMs = msSince(writeStart);
                    wrote = true;
                }
                QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
            });
            last = Clock::now();
            timer.start();
            loop.exec();
            timer.stop();
            maxGap = std::max(maxGap, std::chrono::duration<double, std::milli>(Clock::now() - last).count());
            worker.join();
            row.longestGapMs = maxGap;

            if (const auto *rendered = std::get_if<CaptureRendered>(&*outcome)) {
                row.width = rendered->pixels.width();
                row.height = rendered->pixels.height();
                if (wrote && pngStatus == application::ImageOutputStatus::Succeeded) {
                    row.status = QStringLiteral("rendered");
                    row.pngBytes = QFileInfo(pngPath).size();
                } else {
                    row.status = QStringLiteral("failed");
                }
            } else if (const auto *rejected = std::get_if<CaptureRejected>(&*outcome)) {
                row.status = QStringLiteral("rejected:%1").arg(rejectionName(rejected->rejection.reason));
                if (const auto pixels = captureOutputPixels(rejected->logical, c.percent)) {
                    row.width = pixels->width();
                    row.height = pixels->height();
                }
            }
            row.peakWorkingSet = peakWorkingSet();
            return row;
        }

        // ---- environment -----------------------------------------------------------------------------------

        // The engine is the caller's so the Theme singleton outlives the appearance read from it.
        std::optional<CaptureAppearance> loadAppearance(QQmlEngine &engine) {
            const QString path = QStringLiteral(KSV_SOURCE_DIR "/src/ui/qml/common/Theme.qml");
            qmlRegisterSingletonType(QUrl::fromLocalFile(path), kThemeUri, 1, 0, "Theme");
            QObject *theme = engine.singletonInstance<QObject *>(kThemeUri, "Theme");
            if (!theme) return std::nullopt;

            QQmlComponent labelComponent(&engine);
            labelComponent.setData("import QtQuick.Controls\nLabel {}", QUrl());
            std::unique_ptr<QObject> label(labelComponent.create());
            if (!label) {
                std::cerr << labelComponent.errorString().toStdString() << std::endl;
                return std::nullopt;
            }
            return captureAppearanceFromTheme(*theme, label->property("font").value<QFont>());
        }

        CaptureLimits unlimited(const CaptureLimits &base) {
            constexpr qint64 huge = std::numeric_limits<qint64>::max() / 4;
            CaptureLimits limits = base;
            limits.maxEdgePixels = std::numeric_limits<int>::max();
            limits.maxPixelArea = huge;
            limits.maxEstimatedBytes = huge;
            limits.maxSourcePoints = huge;
            return limits;
        }

        // Only reachable through --interactive-clipboard, which the operator starts by hand.
        int runInteractiveClipboard(const Env &env) {
            std::cout << "This will render overflow-table at 200% and publish it to the system clipboard once,\n"
                         "replacing whatever is on it, and print how long the publish call took.\n"
                         "Press Enter to continue, or close the window to abort." << std::endl;
            std::string line;
            std::getline(std::cin, line);

            const CaptureRenderFunction render = makeCaptureRenderFunction(env.active, env.composition);
            CaptureRenderRequest request;
            request.content = overflowTable(env);
            request.options.percent = 200;
            auto outcome = render(request);
            const auto *rendered = std::get_if<CaptureRendered>(&outcome);
            if (!rendered) {
                std::cerr << "render did not produce an image" << std::endl;
                return 1;
            }
            qt_data::QtClipboardImageWriter writer;
            const auto start = Clock::now();
            const auto status = writer.publish(rendered->raster);
            std::cout << "publish status " << static_cast<int>(status) << " in " << msSince(start) << " ms"
                      << std::endl;
            return status == application::ImageOutputStatus::Succeeded ? 0 : 1;
        }

        // ---- driver ---------------------------------------------------------------------------------------

        int runAll(const std::vector<Case> &cases, const QString &outDir, const bool noLimits) {
            QFile csv(QDir(outDir).absoluteFilePath(QStringLiteral("results.csv")));
            if (!csv.open(QIODevice::WriteOnly | QIODevice::Text)) {
                std::cerr << "cannot write " << csv.fileName().toStdString() << std::endl;
                return 1;
            }
            QTextStream out(&csv);
            out << csvHeader() << '\n';
            std::cout << csvHeader().toStdString() << std::endl;

            for (const Case &c: cases) {
                QStringList args{QStringLiteral("--case"), c.name, QStringLiteral("--out"), outDir};
                if (noLimits) args << QStringLiteral("--no-limits");
                QProcess child;
                child.setProcessChannelMode(QProcess::SeparateChannels);
                child.start(QCoreApplication::applicationFilePath(), args);
                child.waitForFinished(-1);

                QString line;
                const QStringList lines = QString::fromUtf8(child.readAllStandardOutput()).split(QLatin1Char('\n'));
                for (const QString &candidate: lines) {
                    if (candidate.startsWith(c.name + QLatin1Char(','))) line = candidate.trimmed();
                }
                if (line.isEmpty()) line = csvRow(Row{.name = c.name, .scale = c.scale, .percent = c.percent});
                out << line << '\n';
                out.flush();
                std::cout << line.toStdString() << std::endl;
            }
            return 0;
        }
    }
}

int main(int argc, char *argv[]) {
    using namespace ksv::ui;
    QGuiApplication app(argc, argv);

    QString outDir = QDir::current().absoluteFilePath(QStringLiteral("capture-measure"));
    QString caseName;
    bool all = false, list = false, noLimits = false, clipboard = false;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (arg == QLatin1String("--all")) all = true;
        else if (arg == QLatin1String("--list")) list = true;
        else if (arg == QLatin1String("--no-limits")) noLimits = true;
        else if (arg == QLatin1String("--interactive-clipboard")) clipboard = true;
        else if (arg == QLatin1String("--case") && i + 1 < args.size()) caseName = args.at(++i);
        else if (arg == QLatin1String("--out") && i + 1 < args.size()) outDir = QDir::current().absoluteFilePath(args.at(++i));
        else {
            std::cerr << "usage: ksv_capture_measure (--all | --list | --case <name> | --interactive-clipboard)"
                         " [--out <dir>] [--no-limits]" << std::endl;
            return 2;
        }
    }
    if (!QDir().mkpath(outDir)) {
        std::cerr << "cannot create " << outDir.toStdString() << std::endl;
        return 1;
    }

    const std::vector<Case> cases = allCases();
    if (list) {
        for (const Case &c: cases) std::cout << c.name.toStdString() << '\n';
        return 0;
    }
    if (all) return runAll(cases, outDir, noLimits);
    if (caseName.isEmpty() && !clipboard) {
        std::cerr << "choose --all, --list, --case <name> or --interactive-clipboard" << std::endl;
        return 2;
    }

    QQmlEngine engine;
    const auto appearance = loadAppearance(engine);
    if (!appearance) {
        std::cerr << "could not load the Theme singleton" << std::endl;
        return 1;
    }
    Env env;
    env.appearance = *appearance;
    env.provisional = provisionalCaptureLimits();
    env.active = noLimits ? unlimited(env.provisional) : env.provisional;
    env.composition = provisionalCaptureComposition();
    env.outDir = outDir;

    if (clipboard) return runInteractiveClipboard(env);

    const auto found = std::ranges::find(cases, caseName, &Case::name);
    if (found == cases.end()) {
        std::cerr << "unknown case " << caseName.toStdString() << std::endl;
        return 2;
    }
    const Row row = found->percent == 0 ? runSnapshotCase(*found, env) : runRenderCase(*found, env);
    std::cout << csvHeader().toStdString() << '\n' << csvRow(row).toStdString() << std::endl;
    QFile single(QDir(outDir).absoluteFilePath(caseName + QStringLiteral(".csv")));
    if (single.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&single) << csvHeader() << '\n' << csvRow(row) << '\n';
    }
    return 0;
}
