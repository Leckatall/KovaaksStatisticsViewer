#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>
#include <vector>

#include "app/app.h"
#include "capture/capture_appearance.h"
#include "capture/capture_layout.h"
#include "capture/capture_render.h"
#include "capture/capture_sizing.h"
#include "capture/capture_sources.h"
#include "capture/graph_capture_layout.h"
#include "capture/table_capture_layout.h"
#include "components/graph_canvas.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmarks/tracking/benchmark_history_vm.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/capture/capture_vm.h"

#include "integration_env.h"
#include "profile_seeding.h"
#include "../ui/capture/capture_test_support.h"

using namespace ksv;

namespace {
    constexpr long long kDayMs = 24LL * 60 * 60 * 1000;
    constexpr int kWaitMs = 10000;
    constexpr int kTierCount = 8;
    const QString kTrackableId = QStringLiteral("auto-1");
    const QString kOverflowingId = QStringLiteral("overflowing-1");

    constexpr auto kTrackableJson = R"({
        "schemaVersion": 1, "id": "auto-1", "name": "Auto",
        "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] },
                   { "id": "t2", "name": "Silver", "color": [180,180,190,255] } ],
        "uncategorized": [ { "id": "s1", "name": "1wall6targets TE", "hash": null,
            "thresholds": [ { "tierId": "t1", "score": 1000000000.0 }, { "tierId": "t2", "score": 2000000000.0 } ] } ],
        "categories": []
    })";

    // One scenario lacks a tier threshold, which keeps the benchmark incomplete; its table still overflows the window.
    QByteArray overflowingJson() {
        QJsonArray tiers;
        for (int tier = 0; tier < kTierCount; ++tier) {
            tiers.append(QJsonObject{{"id", QStringLiteral("t%1").arg(tier)},
                                     {"name", QStringLiteral("Tier %1").arg(tier)},
                                     {"color", QJsonArray{30 + tier * 20, 90 + tier * 10, 200 - tier * 15, 255}}});
        }
        QJsonArray categories;
        for (int category = 0; category < 3; ++category) {
            QJsonArray scenarios;
            for (int i = 0; i < 20; ++i) {
                const int row = category * 20 + i;
                QJsonArray thresholds;
                for (int tier = 0; tier < kTierCount; ++tier) {
                    if (row == 1 && tier == 3) continue;
                    thresholds.append(QJsonObject{{"tierId", QStringLiteral("t%1").arg(tier)},
                                                  {"score", (row + 1) * 1000.0 + tier * 13.0}});
                }
                const QString hash = QStringLiteral("%1%2").arg(QString(30, 'f')).arg(row + 10);
                scenarios.append(QJsonObject{{"id", QStringLiteral("e%1").arg(row)},
                                             {"name", QStringLiteral("Overflow Drill %1").arg(row)},
                                             {"hash", hash}, {"thresholds", thresholds}});
            }
            categories.append(QJsonObject{{"id", QStringLiteral("c%1").arg(category)},
                                          {"name", QStringLiteral("Category %1").arg(category)},
                                          {"color", QJsonArray{100 + category * 40, 80, 160, 255}},
                                          {"scenarios", scenarios}, {"subcategories", QJsonArray{}}});
        }
        return QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"id", kOverflowingId}, {"name", "Overflowing"},
                                         {"tiers", tiers}, {"uncategorized", QJsonArray{}},
                                         {"categories", categories}})
            .toJson();
    }

    struct CaptureKey {
        const char *key;
        ui::CaptureTarget target;
        int workspaceTab;
    };

    constexpr CaptureKey kKeys[] = {
        {"run", ui::CaptureTarget::Run, 0},
        {"dailyPlaytime", ui::CaptureTarget::DailyPlaytime, 0},
        {"scenarioHistory", ui::CaptureTarget::ScenarioHistory, 0},
        {"benchmarkRank", ui::CaptureTarget::BenchmarkRank, 1},
        {"benchmarkRecentRank", ui::CaptureTarget::BenchmarkRecentRank, 1},
        {"benchmarkPlaytime", ui::CaptureTarget::BenchmarkPlaytime, 1},
        {"benchmarkTable", ui::CaptureTarget::BenchmarkTable, 1},
    };

    [[nodiscard]] QQuickItem *findItem(QQuickItem *parent, const QString &name) {
        if (!parent) return nullptr;
        if (parent->objectName() == name) return parent;
        for (auto *child: parent->childItems()) {
            if (auto *found = findItem(child, name)) return found;
        }
        return nullptr;
    }

    // What a fake writer saw, copied while the image was still alive inside publish()/write().
    struct RecordedImage {
        int width = 0;
        int height = 0;
        std::vector<std::uint32_t> pixels;

        bool operator==(const RecordedImage &) const = default;
    };

    class PixelTap {
    public:
        void attach(tests_support::FakeClipboardImageWriter &writer) {
            writer.setHook([this, &writer] { record(writer.seen().back()); });
        }

        void attach(tests_support::FakePngImageWriter &writer) {
            writer.setHook([this, &writer] { record(writer.seen().back().image); });
        }

        [[nodiscard]] std::vector<RecordedImage> recorded() const {
            std::scoped_lock lock(m_mutex);
            return m_recorded;
        }

    private:
        void record(const tests_support::SeenImage &seen) {
            RecordedImage image{seen.width, seen.height,
                                {seen.pixels, seen.pixels + static_cast<std::size_t>(seen.width) * seen.height}};
            std::scoped_lock lock(m_mutex);
            m_recorded.push_back(std::move(image));
        }

        mutable std::mutex m_mutex;
        std::vector<RecordedImage> m_recorded;
    };

    class CaptureFlow : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        PixelTap clipboardTap;
        PixelTap pngTap;
        QString perfFile;
        long long firstStartMs = 0;

        void SetUp() override {
            integration::clearSavedGraphConfiguration();
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            perfFile = env.copyFixtureIntoPerformances("1wall6targets TE.perf");
            ASSERT_FALSE(perfFile.isEmpty());
            ASSERT_NO_FATAL_FAILURE(writeBenchmark("auto.json", kTrackableJson));
            ASSERT_NO_FATAL_FAILURE(writeBenchmark("overflowing.json", overflowingJson()));
            clipboardTap.attach(*env.clipboardWriter);
            pngTap.attach(*env.pngWriter);
        }

        void TearDown() override {
            env.pngWriter->openGate();
            app.reset();
            integration::clearSavedGraphConfiguration();
        }

        void writeBenchmark(const QString &fileName, const QByteArray &json) const {
            QFile out(QDir(env.benchmarksDir.path()).absoluteFilePath(fileName));
            ASSERT_TRUE(out.open(QIODevice::WriteOnly));
            out.write(json);
        }

        void startApp() {
            app = std::make_unique<application::App>(env.settings, std::make_shared<data::ProtoDecoder>(),
                                                     env.graphConfigurationPersistence, env.statsParser,
                                                     env.benchmarkStore, env.daySource, env.clipboardWriter,
                                                     env.pngWriter);
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000));
            ASSERT_EQ(app->start(), 0);
            ASSERT_NE(window(), nullptr);
        }

        // Runs of the fixture scenario on consecutive days from `firstStart`.
        void seedRuns(const int count, const long long firstStart = 1'772'625'600'000) {
            auto base = app->profileService()->getLatestRun();
            base.run_id.start_time = firstStart;
            domain::UserProfile profile;
            const auto source = profile.ensureSource(env.rootPath().toStdString(), "FPSAimTrainer/performances");
            base.sources.perf = {{source, "1wall6targets TE.perf"}};
            for (int i = 0; i < count; ++i) {
                auto run = base;
                run.run_id.start_time += kDayMs * i;
                if (i == count - 1) run.stored_totals.score = base.totals().score + 50.0F;
                ASSERT_TRUE(profile.addRun(run));
            }
            ASSERT_EQ(tests_support::seedProfile(*app->profileService(), std::move(profile)),
                      application::ProfileImportOutcome::Succeeded);
            firstStartMs = firstStart;
        }

        void setToday(const long long ms) const {
            env.daySource->setToday(std::chrono::sys_days{std::chrono::floor<std::chrono::days>(
                std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{ms}})});
        }

        // Every capture target has content: a run on the graph, seeded history and a trackable benchmark.
        void prepareAllTargets() {
            ASSERT_NO_FATAL_FAILURE(startApp());
            ASSERT_NO_FATAL_FAILURE(seedRuns(5));
            setToday(firstStartMs + 4 * kDayMs);
            app->graphVm()->fetchData(QUrl::fromLocalFile(perfFile).toString());
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->playtimeVm()->hasData(); }, kWaitMs));
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->completionHistoryVm()->runCount() == 5; }, kWaitMs));
            auto *tracking = app->benchmarkTrackingVm();
            tracking->selectBenchmark(kTrackableId);
            ASSERT_TRUE(QTest::qWaitFor([&] { return tracking->state() == "trackable"; }, kWaitMs));
            ASSERT_TRUE(QTest::qWaitFor([&] {
                return tracking->rankHistory()->hasData() && tracking->recentRankHistory()->hasData() &&
                       tracking->playtimeHistory()->hasData();
            }, kWaitMs));
        }

        [[nodiscard]] QQuickWindow *window() const {
            return app ? qobject_cast<QQuickWindow *>(app->engine()->rootObjects().first()) : nullptr;
        }

        [[nodiscard]] QQuickItem *find(const QString &name) const { return findItem(window()->contentItem(), name); }

        [[nodiscard]] QQuickWindow *previewWindow() const {
            return window()->findChild<QQuickWindow *>("capturePreviewDialog");
        }

        [[nodiscard]] QQuickItem *findInPreview(const QString &name) const {
            auto *preview = previewWindow();
            return preview ? findItem(preview->contentItem(), name) : nullptr;
        }

        void showWorkspace(const int index) const {
            auto *tabs = find("workspaceTabBar");
            ASSERT_NE(tabs, nullptr);
            tabs->setProperty("currentIndex", index);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }

        static void click(QQuickItem *button) {
            ASSERT_NE(button, nullptr);
            ASSERT_TRUE(button->isEnabled());
            ASSERT_TRUE(QMetaObject::invokeMethod(button, "click"));
        }

        QQuickItem *tableFlickable = nullptr;
        QQuickItem *tableInnerFlickable = nullptr;

        // Shows the overflowing benchmark's table and locates the two flickables that scroll it.
        void showOverflowingTable() {
            auto *tracking = app->benchmarkTrackingVm();
            tracking->selectBenchmark(kOverflowingId);
            ASSERT_TRUE(QTest::qWaitFor([&] { return tracking->state() == "incomplete"; }, kWaitMs))
                << tracking->state().toStdString();
            ASSERT_NO_FATAL_FAILURE(showWorkspace(1));

            QQuickItem *grid = nullptr;
            ASSERT_TRUE(QTest::qWaitFor([&] { return (grid = find("benchmarkProgressGrid")) != nullptr; }, kWaitMs));
            QQuickItem *scroll = grid;
            while (scroll && !scroll->inherits("QQuickScrollView")) scroll = scroll->parentItem();
            ASSERT_NE(scroll, nullptr);
            tableFlickable = scroll->property("contentItem").value<QQuickItem *>();
            ASSERT_NE(tableFlickable, nullptr);
            tableInnerFlickable = findItem(grid, "progressGridFlick");
            ASSERT_NE(tableInnerFlickable, nullptr);
            ASSERT_TRUE(QTest::qWaitFor([&] {
                return tableFlickable->property("contentHeight").toReal() > tableFlickable->height();
            }, kWaitMs)) << "the table must overflow the viewport vertically";
        }

        [[nodiscard]] QPointF tableOffsets() const {
            return {tableInnerFlickable->property("contentX").toReal(),
                    tableFlickable->property("contentY").toReal()};
        }

        [[nodiscard]] QPointF tableFarCorner() const {
            return {std::max<qreal>(0.0, tableInnerFlickable->property("contentWidth").toReal()
                                             - tableInnerFlickable->width()),
                    tableFlickable->property("contentHeight").toReal() - tableFlickable->height()};
        }

        void scrollTableTo(const QPointF &offsets) const {
            tableInnerFlickable->setProperty("contentX", offsets.x());
            tableFlickable->setProperty("contentY", offsets.y());
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }

        [[nodiscard]] presentation::CaptureViewModel *captureVm() const { return app->captureVm(); }

        [[nodiscard]] bool awaitReadyPreview() const {
            return QTest::qWaitFor([&] { return captureVm()->previewState() == "ready"; }, kWaitMs);
        }

        [[nodiscard]] QSize outputSize() const {
            return {captureVm()->outputWidth(), captureVm()->outputHeight()};
        }

        [[nodiscard]] ui::CaptureAppearance liveAppearance() const {
            auto *theme = app->engine()->singletonInstance<QObject *>("KovaaksStatsViewer", "Theme");
            EXPECT_NE(theme, nullptr);
            return ui::captureAppearanceFromTheme(*theme, window()->property("font").value<QFont>());
        }

        [[nodiscard]] bool awaitCopyOutcome(const std::size_t publishedBefore, const QString &targetName) const {
            return QTest::qWaitFor([&] {
                return env.clipboardWriter->publishCount() == publishedBefore + 1 &&
                       captureVm()->feedbackKind() == "success" &&
                       captureVm()->feedbackText().contains(targetName);
            }, kWaitMs);
        }

        [[nodiscard]] static ui::GraphCanvas *canvasOf(const QQuickItem *camera) {
            return qobject_cast<ui::GraphCanvas *>(camera->parentItem()->property("canvas").value<QObject *>());
        }

        [[nodiscard]] ui::GraphCaptureContext contextFor(const ui::CaptureTarget target) const {
            auto *tracking = app->benchmarkTrackingVm();
            switch (target) {
                case ui::CaptureTarget::Run: return ui::runCaptureContext(*app->graphVm());
                case ui::CaptureTarget::DailyPlaytime: return ui::dailyPlaytimeCaptureContext(*app->playtimeVm());
                case ui::CaptureTarget::ScenarioHistory:
                    return ui::scenarioHistoryCaptureContext(*app->completionHistoryVm());
                case ui::CaptureTarget::BenchmarkRank:
                    return ui::benchmarkHistoryCaptureContext(tracking->selectedName(), *tracking->rankHistory());
                case ui::CaptureTarget::BenchmarkRecentRank:
                    return ui::benchmarkHistoryCaptureContext(tracking->selectedName(),
                                                              *tracking->recentRankHistory());
                case ui::CaptureTarget::BenchmarkPlaytime:
                    return ui::benchmarkHistoryCaptureContext(tracking->selectedName(), *tracking->playtimeHistory());
                case ui::CaptureTarget::BenchmarkTable: break;
            }
            return {};
        }

        // The pixels a capture of this target would have at `percent`, derived without the capture view model.
        [[nodiscard]] std::optional<QSize> expectedPixels(const CaptureKey &entry, const QQuickItem *camera,
                                                          const int percent) const {
            const ui::CaptureAppearance appearance = liveAppearance();
            if (entry.target == ui::CaptureTarget::BenchmarkTable) {
                const auto snapshot = ui::captureTable(*app->benchmarkTrackingVm(), appearance);
                if (!snapshot) return std::nullopt;
                return ui::captureOutputPixels(ui::layoutTableCapture(*snapshot, true).size, percent);
            }
            const ui::GraphCanvas *canvas = canvasOf(camera);
            if (!canvas) return std::nullopt;
            const auto snapshot = ui::captureGraph(entry.target, *canvas, contextFor(entry.target), appearance);
            if (!snapshot) return std::nullopt;
            return ui::captureOutputPixels(ui::graphReferenceSize(*snapshot, ui::provisionalCaptureComposition()),
                                           percent);
        }
    };

    // --- I1 -------------------------------------------------------------

    TEST_F(CaptureFlow, EveryTargetCopiesFromItsCamera) {
        ASSERT_NO_FATAL_FAILURE(prepareAllTargets());

        std::size_t published = 0;
        for (const CaptureKey &entry: kKeys) {
            SCOPED_TRACE(entry.key);
            ASSERT_NO_FATAL_FAILURE(showWorkspace(entry.workspaceTab));
            QQuickItem *camera = find(QStringLiteral("captureCopy_") + entry.key);
            ASSERT_NE(camera, nullptr);
            const auto expected = expectedPixels(entry, camera, 200);
            ASSERT_TRUE(expected.has_value());

            ASSERT_NO_FATAL_FAILURE(click(camera));
            ASSERT_TRUE(awaitCopyOutcome(published, ui::captureTargetName(entry.target)));
            ++published;

            const auto seen = env.clipboardWriter->seen();
            ASSERT_EQ(seen.size(), published);
            EXPECT_EQ(QSize(seen.back().width, seen.back().height), *expected);
            EXPECT_FALSE(captureVm()->previewOpen());
            EXPECT_TRUE(previewWindow() == nullptr || !previewWindow()->isVisible());
            EXPECT_EQ(env.pngWriter->writeCount(), 0U);
        }
    }

    TEST_F(CaptureFlow, TheCameraIsReachableByKeyboard) {
        ASSERT_NO_FATAL_FAILURE(prepareAllTargets());
        ASSERT_NO_FATAL_FAILURE(showWorkspace(0));
        QQuickItem *camera = find("captureCopy_run");
        ASSERT_NE(camera, nullptr);

        window()->requestActivate();
        ASSERT_TRUE(QTest::qWaitFor([&] { return window()->isActive(); }, 3000));
        camera->forceActiveFocus(Qt::TabFocusReason);
        ASSERT_TRUE(camera->hasActiveFocus());

        QTest::keyClick(window(), Qt::Key_Space);
        ASSERT_TRUE(awaitCopyOutcome(0, ui::captureTargetName(ui::CaptureTarget::Run)));
        EXPECT_EQ(env.clipboardWriter->publishCount(), 1U);
    }

    TEST_F(CaptureFlow, AGraphActionCapturesWhatTheCanvasPaints) {
        ASSERT_NO_FATAL_FAILURE(prepareAllTargets());
        ASSERT_NO_FATAL_FAILURE(showWorkspace(0));
        QQuickItem *camera = find("captureCopy_run");
        ASSERT_NE(camera, nullptr);
        ui::GraphCanvas *canvas = canvasOf(camera);
        ASSERT_NE(canvas, nullptr);

        auto *visualSettings = window()->findChild<QObject *>("visualSettings");
        ASSERT_NE(visualSettings, nullptr);
        const qsizetype allSeries = canvas->scene().series.size();
        ASSERT_GE(allSeries, 2);
        QVariantList visible = app->graphVm()->enabledSeriesIds();
        visible.removeLast();
        visualSettings->setProperty("visibleSeriesIds", visible);
        ASSERT_TRUE(QTest::qWaitFor([&] { return canvas->scene().series.size() < allSeries; }, kWaitMs));

        std::mutex logMutex;
        std::vector<ui::CaptureRenderRequest> requests;
        const auto inner = ui::makeCaptureRenderFunction(ui::provisionalCaptureLimits(),
                                                         ui::provisionalCaptureComposition());
        presentation::CaptureViewModel second(
            app->imageOutputUseCase(),
            {app->graphVm(), app->playtimeVm(), app->completionHistoryVm(), app->benchmarkTrackingVm()},
            [&](const ui::CaptureRenderRequest &request) {
                {
                    std::scoped_lock lock(logMutex);
                    requests.push_back(request);
                }
                return inner(request);
            });
        second.useTheme(app->engine()->singletonInstance<QObject *>("KovaaksStatsViewer", "Theme"),
                        window()->property("font").value<QFont>());

        const ui::GraphScene live = canvas->scene();
        second.quickCopy("run", canvas);
        ASSERT_TRUE(QTest::qWaitFor([&] { return second.feedbackKind() == "success"; }, kWaitMs));

        ASSERT_EQ(requests.size(), 1U);
        const auto *snapshot = std::get_if<ui::GraphCaptureSnapshot>(requests.front().content.get());
        ASSERT_NE(snapshot, nullptr);
        EXPECT_FALSE(snapshot->scene.selectedBand.has_value());
        ASSERT_EQ(snapshot->scene.series.size(), live.series.size());
        for (qsizetype i = 0; i < live.series.size(); ++i) {
            const auto &captured = snapshot->scene.series.at(i);
            const auto &painted = live.series.at(i);
            EXPECT_EQ(captured.column, painted.column);
            EXPECT_EQ(captured.id, painted.id);
            EXPECT_EQ(captured.name, painted.name);
            EXPECT_EQ(captured.color, painted.color);
            EXPECT_EQ(captured.observations, painted.observations);
        }
        const ui::GraphCaptureContext context = ui::runCaptureContext(*app->graphVm());
        EXPECT_EQ(snapshot->context.title, context.title);
        EXPECT_EQ(snapshot->context.xTitle, context.xTitle);
        EXPECT_EQ(snapshot->context.rangeText, context.rangeText);
        EXPECT_FALSE(snapshot->context.title.isEmpty());
    }

    TEST_F(CaptureFlow, TheTableIgnoresScrollPosition) {
        ASSERT_NO_FATAL_FAILURE(startApp());
        ASSERT_NO_FATAL_FAILURE(showOverflowingTable());

        QQuickItem *camera = find("captureCopy_benchmarkTable");
        ASSERT_NE(camera, nullptr);
        std::vector<QPointF> scrolledTo;
        for (const QPointF &target: {QPointF(0.0, 0.0), tableFarCorner()}) {
            scrollTableTo(target);
            scrolledTo.push_back(tableOffsets());

            const std::size_t before = env.clipboardWriter->publishCount();
            captureVm()->dismissFeedback();
            ASSERT_NO_FATAL_FAILURE(click(camera));
            ASSERT_TRUE(awaitCopyOutcome(before, ui::captureTargetName(ui::CaptureTarget::BenchmarkTable)));
        }
        EXPECT_NE(scrolledTo.front(), scrolledTo.back());

        const auto images = clipboardTap.recorded();
        ASSERT_EQ(images.size(), 2U);
        EXPECT_EQ(images.front().width, images.back().width);
        EXPECT_EQ(images.front().height, images.back().height);
        EXPECT_TRUE(images.front() == images.back());
    }

    // --- I2 -------------------------------------------------------------

    TEST_F(CaptureFlow, LiveUpdatesDoNotReachAnOpenPreview) {
        ASSERT_NO_FATAL_FAILURE(prepareAllTargets());
        ASSERT_NO_FATAL_FAILURE(showWorkspace(0));
        QQuickItem *camera = find("captureCopy_dailyPlaytime");
        ASSERT_NE(camera, nullptr);
        ui::GraphCanvas *canvas = canvasOf(camera);
        ASSERT_NE(canvas, nullptr);
        const ui::CaptureAppearance appearance = liveAppearance();
        const auto reviewed = ui::captureGraph(ui::CaptureTarget::DailyPlaytime, *canvas,
                                               ui::dailyPlaytimeCaptureContext(*app->playtimeVm()), appearance);
        ASSERT_TRUE(reviewed.has_value());
        const QSizeF reference = ui::graphReferenceSize(*reviewed, ui::provisionalCaptureComposition());

        captureVm()->openPreview("dailyPlaytime", canvas);
        ASSERT_TRUE(awaitReadyPreview());
        EXPECT_EQ(outputSize(), ui::captureOutputPixels(reference, 200));

        const long long laterStart = firstStartMs + 40 * kDayMs;
        ASSERT_NO_FATAL_FAILURE(seedRuns(3, laterStart));
        setToday(laterStart + 2 * kDayMs);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            const auto now = ui::captureGraph(ui::CaptureTarget::DailyPlaytime, *canvas,
                                              ui::dailyPlaytimeCaptureContext(*app->playtimeVm()), appearance);
            return now && now->context.rangeText != reviewed->context.rangeText;
        }, kWaitMs)) << "the live playtime graph did not move on";

        captureVm()->setPercent(300);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return captureVm()->previewState() == "ready" && captureVm()->percent() == 300;
        }, kWaitMs));
        EXPECT_EQ(outputSize(), ui::captureOutputPixels(reference, 300));
    }

    TEST_F(CaptureFlow, ClipboardAndPngMatchTheReportedDimensions) {
        ASSERT_NO_FATAL_FAILURE(prepareAllTargets());
        ASSERT_NO_FATAL_FAILURE(showWorkspace(0));
        QQuickItem *camera = find("captureCopy_run");
        ASSERT_NE(camera, nullptr);

        captureVm()->openPreview("run", canvasOf(camera));
        ASSERT_TRUE(awaitReadyPreview());
        captureVm()->setPercent(300);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return captureVm()->previewState() == "ready" && captureVm()->percent() == 300;
        }, kWaitMs));
        constexpr int kCustomWidth = 1800;
        captureVm()->commitWidth(QString::number(kCustomWidth));
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return captureVm()->previewState() == "ready" && captureVm()->outputWidth() == kCustomWidth;
        }, kWaitMs));
        const QSize reported = outputSize();

        ASSERT_NO_FATAL_FAILURE(click(findInPreview("captureCopyButton")));
        ASSERT_TRUE(awaitCopyOutcome(0, ui::captureTargetName(ui::CaptureTarget::Run)));
        EXPECT_TRUE(captureVm()->previewOpen());

        QTemporaryDir out;
        ASSERT_TRUE(out.isValid());
        const QString destination = QDir(out.path()).absoluteFilePath("reviewed.png");
        ASSERT_TRUE(captureVm()->beginExport());
        captureVm()->confirmExport(QUrl::fromLocalFile(destination));
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return env.pngWriter->writeCount() == 1 && captureVm()->feedbackText().contains("exported");
        }, kWaitMs));

        const auto copied = clipboardTap.recorded();
        const auto saved = pngTap.recorded();
        ASSERT_EQ(copied.size(), 1U);
        ASSERT_EQ(saved.size(), 1U);
        EXPECT_EQ(QSize(copied.front().width, copied.front().height), reported);
        EXPECT_EQ(QSize(saved.front().width, saved.front().height), reported);
        EXPECT_TRUE(copied.front() == saved.front());
    }

    TEST_F(CaptureFlow, TheSummaryOptionKeepsTheTitleAndHeadings) {
        ASSERT_NO_FATAL_FAILURE(prepareAllTargets());
        ASSERT_NO_FATAL_FAILURE(showWorkspace(1));

        captureVm()->openPreview("benchmarkTable", nullptr);
        ASSERT_TRUE(awaitReadyPreview());
        ASSERT_TRUE(captureVm()->summaryAvailable());
        ASSERT_TRUE(captureVm()->includeSummary());
        const QSize withSummary = outputSize();

        captureVm()->setIncludeSummary(false);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return captureVm()->previewState() == "ready" && !captureVm()->includeSummary();
        }, kWaitMs));
        const QSize withoutSummary = outputSize();
        EXPECT_LT(withoutSummary.height(), withSummary.height());

        const auto snapshot = ui::captureTable(*app->benchmarkTrackingVm(), liveAppearance());
        ASSERT_TRUE(snapshot.has_value());
        const ui::CaptureLayout full = ui::layoutTableCapture(*snapshot, true);
        const ui::CaptureLayout bare = ui::layoutTableCapture(*snapshot, false);
        EXPECT_EQ(withSummary, ui::captureOutputPixels(full.size, 200));
        EXPECT_EQ(withoutSummary, ui::captureOutputPixels(bare.size, 200));

        QStringList bareTexts;
        for (const auto &item: ui::captureTextItems(bare.items)) bareTexts << item.text;
        EXPECT_TRUE(bareTexts.contains(snapshot->benchmarkName));
        for (const QString &heading: {snapshot->grid.headings.category, snapshot->grid.headings.subcategory,
                                      snapshot->grid.headings.scenario}) {
            QStringList fullTexts;
            for (const auto &item: ui::captureTextItems(full.items)) fullTexts << item.text;
            if (fullTexts.contains(heading)) EXPECT_TRUE(bareTexts.contains(heading)) << heading.toStdString();
        }
        EXPECT_TRUE(bareTexts.contains(snapshot->grid.headings.scenario));
    }

    [[nodiscard]] QVariantMap savedGraphSettings() {
        QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        QVariantMap values;
        for (const QString &key: raw.allKeys()) {
            if (key.startsWith("graph")) values.insert(key, raw.value(key));
        }
        return values;
    }

    [[nodiscard]] QByteArray fileBytes(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    [[nodiscard]] QStringList filesUnder(const QString &root) {
        QStringList files;
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) files << it.next();
        files.sort();
        return files;
    }

    TEST_F(CaptureFlow, CapturingLeavesTheSourceAlone) {
        ASSERT_NO_FATAL_FAILURE(startApp());
        ASSERT_NO_FATAL_FAILURE(showOverflowingTable());
        const QPointF target = tableFarCorner() / 2.0;
        scrollTableTo(target);
        const QPointF scrolled = tableOffsets();
        ASSERT_NE(scrolled, QPointF(0.0, 0.0));

        const QByteArray profileBefore = fileBytes(env.profileStorePath());
        const QVariantMap graphBefore = savedGraphSettings();

        captureVm()->openPreview("benchmarkTable", nullptr);
        ASSERT_TRUE(awaitReadyPreview());
        captureVm()->setPercent(400);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return captureVm()->previewState() == "ready" && captureVm()->percent() == 400;
        }, kWaitMs));
        const QSize at400 = outputSize();
        captureVm()->closePreview();

        EXPECT_EQ(fileBytes(env.profileStorePath()), profileBefore);
        EXPECT_EQ(savedGraphSettings(), graphBefore);
        EXPECT_EQ(tableOffsets(), scrolled);

        ASSERT_NO_FATAL_FAILURE(click(find("captureCopy_benchmarkTable")));
        ASSERT_TRUE(awaitCopyOutcome(0, ui::captureTargetName(ui::CaptureTarget::BenchmarkTable)));
        const auto copied = env.clipboardWriter->seen();
        ASSERT_EQ(copied.size(), 1U);
        const auto snapshot = ui::captureTable(*app->benchmarkTrackingVm(), liveAppearance());
        ASSERT_TRUE(snapshot.has_value());
        const QSize quick(copied.front().width, copied.front().height);
        EXPECT_EQ(quick, ui::captureOutputPixels(ui::layoutTableCapture(*snapshot, true).size, 200));
        EXPECT_NE(quick, at400);
        EXPECT_EQ(tableOffsets(), scrolled);
    }

    TEST_F(CaptureFlow, AnOutputReachesOnlyTheInjectedLeaves) {
        ASSERT_NO_FATAL_FAILURE(prepareAllTargets());
        ASSERT_NO_FATAL_FAILURE(showWorkspace(0));
        QTemporaryDir out;
        ASSERT_TRUE(out.isValid());
        const QString destination = QDir(out.path()).absoluteFilePath("only-here.png");
        const QStringList rootBefore = filesUnder(env.rootPath());
        const QStringList benchmarksBefore = filesUnder(env.benchmarksDir.path());

        ASSERT_NO_FATAL_FAILURE(click(find("captureCopy_run")));
        ASSERT_TRUE(awaitCopyOutcome(0, ui::captureTargetName(ui::CaptureTarget::Run)));

        captureVm()->openPreview("run", canvasOf(find("captureCopy_run")));
        ASSERT_TRUE(awaitReadyPreview());
        ASSERT_TRUE(captureVm()->beginExport());
        captureVm()->confirmExport(QUrl::fromLocalFile(destination));
        ASSERT_TRUE(QTest::qWaitFor([&] { return captureVm()->feedbackText().contains("exported"); }, kWaitMs));

        EXPECT_EQ(env.clipboardWriter->publishCount(), 1U);
        const auto writes = env.pngWriter->seen();
        ASSERT_EQ(writes.size(), 1U);
        EXPECT_EQ(QString::fromStdString(writes.front().destination.generic_string()),
                  QDir::fromNativeSeparators(destination));
        EXPECT_TRUE(filesUnder(out.path()).isEmpty());
        EXPECT_EQ(filesUnder(env.rootPath()), rootBefore);
        EXPECT_EQ(filesUnder(env.benchmarksDir.path()), benchmarksBefore);
    }

    // --- I3 -------------------------------------------------------------

    class CaptureFlowFailures : public CaptureFlow {
    protected:
        // The run graph holds the fixture run, so the run target has content.
        void showRun() {
            ASSERT_NO_FATAL_FAILURE(startApp());
            app->graphVm()->fetchData(QUrl::fromLocalFile(perfFile).toString());
            ASSERT_EQ(app->graphVm()->contentState(), presentation::GraphViewModel::HasData);
            ASSERT_NO_FATAL_FAILURE(showWorkspace(0));
            ASSERT_TRUE(QTest::qWaitFor([&] { return find("captureCopy_run") != nullptr; }, kWaitMs));
        }

        void openRunPreview() {
            captureVm()->openPreview("run", canvasOf(find("captureCopy_run")));
            ASSERT_TRUE(awaitReadyPreview());
        }

        [[nodiscard]] QString destination() {
            if (!m_out.isValid()) return {};
            return QDir(m_out.path()).absoluteFilePath("saved.png");
        }

    private:
        QTemporaryDir m_out;
    };

    TEST_F(CaptureFlowFailures, ClipboardFailureKeepsThePreviewForRetry) {
        ASSERT_NO_FATAL_FAILURE(showRun());
        ASSERT_NO_FATAL_FAILURE(openRunPreview());
        env.clipboardWriter->setStatus(application::ImageOutputStatus::ClipboardNotConfirmed);

        QQuickItem *copy = findInPreview("captureCopyButton");
        ASSERT_NE(copy, nullptr);
        ASSERT_NO_FATAL_FAILURE(click(copy));
        ASSERT_TRUE(QTest::qWaitFor([&] { return captureVm()->feedbackKind() == "failure"; }, kWaitMs));

        QQuickItem *footerText = findInPreview("captureStatusText");
        ASSERT_NE(footerText, nullptr);
        EXPECT_TRUE(footerText->isVisible());
        EXPECT_EQ(footerText->property("text").toString(), captureVm()->feedbackText());
        EXPECT_TRUE(captureVm()->feedbackText().contains("Could not copy"));
        EXPECT_TRUE(captureVm()->previewOpen());
        EXPECT_TRUE(copy->isEnabled());
        EXPECT_TRUE(captureVm()->canOutput());

        env.clipboardWriter->setStatus(application::ImageOutputStatus::Succeeded);
        ASSERT_NO_FATAL_FAILURE(click(copy));
        ASSERT_TRUE(QTest::qWaitFor([&] { return captureVm()->feedbackKind() == "success"; }, kWaitMs));
        EXPECT_EQ(env.clipboardWriter->publishCount(), 2U);
        EXPECT_TRUE(captureVm()->previewOpen());
    }

    TEST_F(CaptureFlowFailures, SaveFailureReachesTheMainFooter) {
        ASSERT_NO_FATAL_FAILURE(showRun());
        ASSERT_NO_FATAL_FAILURE(openRunPreview());
        const QString defaultWidth = captureVm()->widthText();
        captureVm()->setPercent(300);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return captureVm()->previewState() == "ready" && captureVm()->percent() == 300;
        }, kWaitMs));
        env.pngWriter->setStatus(application::ImageOutputStatus::CommitFailed);

        ASSERT_TRUE(captureVm()->beginExport());
        captureVm()->confirmExport(QUrl::fromLocalFile(destination()));
        ASSERT_TRUE(QTest::qWaitFor([&] { return captureVm()->feedbackKind() == "failure"; }, kWaitMs));

        EXPECT_FALSE(captureVm()->previewOpen());
        EXPECT_TRUE(previewWindow() == nullptr || !previewWindow()->isVisible());
        QQuickItem *bar = find("profileStatusBar");
        ASSERT_NE(bar, nullptr);
        QQuickItem *text = findItem(bar, "captureStatusText");
        QQuickItem *dismiss = findItem(bar, "captureStatusDismiss");
        ASSERT_NE(text, nullptr);
        ASSERT_NE(dismiss, nullptr);
        EXPECT_TRUE(text->isVisible());
        EXPECT_TRUE(dismiss->isVisible());
        EXPECT_TRUE(text->property("text").toString().contains("Could not export"));

        ASSERT_NO_FATAL_FAILURE(click(dismiss));
        EXPECT_EQ(captureVm()->feedbackKind(), "none");
        EXPECT_FALSE(text->isVisible());

        captureVm()->openPreview("run", canvasOf(find("captureCopy_run")));
        ASSERT_TRUE(awaitReadyPreview());
        EXPECT_EQ(captureVm()->percent(), 200);
        EXPECT_EQ(captureVm()->widthText(), defaultWidth);
        EXPECT_EQ(env.pngWriter->writeCount(), 1U);
    }

    TEST_F(CaptureFlowFailures, ASaveInProgressDoesNotDelayTheUi) {
        ASSERT_NO_FATAL_FAILURE(showRun());
        ASSERT_NO_FATAL_FAILURE(openRunPreview());
        env.pngWriter->closeGate();
        ASSERT_TRUE(captureVm()->beginExport());
        captureVm()->confirmExport(QUrl::fromLocalFile(destination()));
        ASSERT_TRUE(QTest::qWaitFor([&] { return env.pngWriter->writeCount() == 1; }, kWaitMs));
        EXPECT_FALSE(captureVm()->previewOpen());

        QTest::qWait(100);
        ASSERT_NO_FATAL_FAILURE(click(find("captureCopy_run")));
        ASSERT_TRUE(QTest::qWaitFor([&] { return captureVm()->feedbackKind() == "notice"; }, kWaitMs));
        EXPECT_TRUE(captureVm()->feedbackText().contains("still saving"));
        EXPECT_EQ(env.clipboardWriter->publishCount(), 0U);
        EXPECT_EQ(env.pngWriter->writeCount(), 1U);
        EXPECT_NE(captureVm()->feedbackKind(), "success");

        env.pngWriter->openGate();
        ASSERT_TRUE(QTest::qWaitFor([&] { return captureVm()->feedbackKind() == "success"; }, kWaitMs));
        EXPECT_TRUE(captureVm()->feedbackText().contains("exported"));
        EXPECT_EQ(env.clipboardWriter->publishCount(), 0U);
    }

    TEST_F(CaptureFlowFailures, CaptureFeedbackLeavesImportWarningsVisible) {
        QFile broken(QDir(env.performancesDir()).absoluteFilePath("broken.perf"));
        ASSERT_TRUE(broken.open(QIODevice::WriteOnly));
        broken.write("this is not a perf file");
        broken.close();

        ASSERT_NO_FATAL_FAILURE(showRun());
        app->sessionVm()->refreshProfile();
        QQuickItem *bar = find("profileStatusBar");
        ASSERT_NE(bar, nullptr);
        QQuickItem *details = findItem(bar, "profileStatusDetailsButton");
        QQuickItem *warning = findItem(bar, "profileStatusText");
        ASSERT_NE(details, nullptr);
        ASSERT_NE(warning, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] { return details->isVisible(); }, kWaitMs))
            << "no unresolved import warning appeared";

        ASSERT_NO_FATAL_FAILURE(click(find("captureCopy_run")));
        ASSERT_TRUE(QTest::qWaitFor([&] { return captureVm()->feedbackKind() == "success"; }, kWaitMs));

        QQuickItem *dismissWarning = findItem(bar, "profileStatusDismissButton");
        QQuickItem *captureText = findItem(bar, "captureStatusText");
        ASSERT_NE(dismissWarning, nullptr);
        ASSERT_NE(captureText, nullptr);
        EXPECT_TRUE(warning->isVisible());
        EXPECT_TRUE(details->isVisible());
        EXPECT_TRUE(dismissWarning->isVisible());
        EXPECT_TRUE(captureText->isVisible());
        EXPECT_TRUE(captureText->property("text").toString().contains("copied"));
        // The strip's slot only exists once the bar's layout has been polished after it became visible.
        QQuickItem *strip = captureText->parentItem()->parentItem();
        const auto sceneRect = [](const QQuickItem *item) { return item->mapRectToScene(item->boundingRect()); };
        EXPECT_TRUE(QTest::qWaitFor([&] {
            return strip->width() > 0 && !sceneRect(warning).intersects(sceneRect(strip));
        }, 2000)) << "the capture message shares the warning's slot";
        EXPECT_GT(sceneRect(warning).width(), 0.0);
        EXPECT_GT(sceneRect(captureText).width(), 0.0);

        ASSERT_TRUE(QTest::qWaitFor([&] { return captureVm()->feedbackKind() == "none"; }, kWaitMs));
        EXPECT_FALSE(captureText->isVisible());
        EXPECT_TRUE(warning->isVisible());
        EXPECT_TRUE(details->isVisible());
        EXPECT_FALSE(warning->property("text").toString().isEmpty());
    }

    TEST_F(CaptureFlowFailures, ClosingDuringASaveLetsItFinish) {
        ASSERT_NO_FATAL_FAILURE(showRun());
        ASSERT_NO_FATAL_FAILURE(openRunPreview());
        env.pngWriter->closeGate();
        ASSERT_TRUE(captureVm()->beginExport());
        captureVm()->confirmExport(QUrl::fromLocalFile(destination()));
        ASSERT_TRUE(QTest::qWaitFor([&] { return env.pngWriter->writeCount() == 1; }, kWaitMs));
        ASSERT_TRUE(pngTap.recorded().empty());

        std::thread opener([writer = env.pngWriter] {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            writer->openGate();
        });
        app.reset();
        opener.join();

        EXPECT_EQ(pngTap.recorded().size(), 1U);
        EXPECT_EQ(env.pngWriter->writeCount(), 1U);
        EXPECT_EQ(env.clipboardWriter->publishCount(), 0U);
    }
}
