#include <gtest/gtest.h>

#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <variant>

#include "app/app.h"
#include "app/contracts/graph_configuration.h"
#include "capture/capture_sizing.h"
#include "capture/capture_renderer.h"
#include "capture/capture_sources.h"
#include "capture/graph_capture_layout.h"
#include "components/graph_canvas.h"
#include "components/graph_scene_painter.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"

#include "integration_env.h"
#include "profile_seeding.h"

using namespace ksv;

namespace {
    constexpr long long kDayMs = 24LL * 60 * 60 * 1000;
    constexpr int kPaintBudget = 2000;

    constexpr auto kBenchmarkJson = R"({
        "schemaVersion": 1, "id": "auto-1", "name": "Auto",
        "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] },
                   { "id": "t2", "name": "Silver", "color": [180,180,190,255] } ],
        "uncategorized": [ { "id": "s1", "name": "1wall6targets TE", "hash": null,
            "thresholds": [ { "tierId": "t1", "score": 1000000000.0 }, { "tierId": "t2", "score": 2000000000.0 } ] } ],
        "categories": []
    })";

    ui::CaptureAppearance testAppearance() {
        ui::CaptureAppearance appearance;
        appearance.window = QColor(30, 30, 34);
        appearance.base = QColor(45, 45, 45);
        appearance.text = QColor(240, 240, 240);
        appearance.button = QColor(60, 60, 66);
        appearance.buttonText = QColor(240, 240, 240);
        appearance.mid = QColor(90, 90, 96);
        appearance.placeholderText = QColor(160, 160, 166);
        appearance.highlight = QColor(66, 217, 232);
        appearance.attention = QColor(230, 120, 40);
        const QFont font = QGuiApplication::font();
        appearance.font.families = font.families().isEmpty() ? QStringList{font.family()} : font.families();
        appearance.font.pixelSize = 14;
        return appearance;
    }

    std::unique_ptr<ui::GraphCanvas> makeCanvas(presentation::GraphViewModelBase &vm, const QVariantList &columns) {
        auto canvas = std::make_unique<ui::GraphCanvas>();
        canvas->setWidth(800);
        canvas->setHeight(400);
        canvas->setGraphVm(&vm);
        canvas->setVisibleColumns(columns);
        canvas->setConnectionMode(static_cast<ui::GraphCanvas::ConnectionMode>(vm.connectionMode()));
        canvas->setPointMarkers(vm.pointMarkers());
        return canvas;
    }

    void expectSameAxis(const ui::GraphSceneAxis &actual, const ui::GraphSceneAxis &expected) {
        EXPECT_EQ(actual.min, expected.min);
        EXPECT_EQ(actual.max, expected.max);
        EXPECT_EQ(actual.ticks, expected.ticks);
        EXPECT_EQ(actual.labels, expected.labels);
        EXPECT_EQ(actual.minLabel, expected.minLabel);
        EXPECT_EQ(actual.maxLabel, expected.maxLabel);
    }

    void expectSameScene(const ui::GraphScene &actual, const ui::GraphScene &expected) {
        ASSERT_EQ(actual.series.size(), expected.series.size());
        for (qsizetype i = 0; i < expected.series.size(); ++i) {
            const auto &a = actual.series.at(i);
            const auto &e = expected.series.at(i);
            EXPECT_EQ(a.column, e.column);
            EXPECT_EQ(a.id, e.id);
            EXPECT_EQ(a.name, e.name);
            EXPECT_EQ(a.color, e.color);
            EXPECT_EQ(a.lineWidth, e.lineWidth);
            EXPECT_EQ(a.observations, e.observations);
            EXPECT_EQ(a.leadingCarry, e.leadingCarry);
            EXPECT_EQ(a.trailingCarry, e.trailingCarry);
            EXPECT_EQ(a.yAxisId, e.yAxisId);
            EXPECT_EQ(a.references.size(), e.references.size());
            expectSameAxis(a.xAxis, e.xAxis);
            expectSameAxis(a.yAxis, e.yAxis);
        }
        expectSameAxis(actual.xAxis, expected.xAxis);
        EXPECT_EQ(actual.labelledYAxis.has_value(), expected.labelledYAxis.has_value());
        if (actual.labelledYAxis && expected.labelledYAxis) expectSameAxis(*actual.labelledYAxis, *expected.labelledYAxis);
        EXPECT_EQ(actual.labelledColumn, expected.labelledColumn);
        EXPECT_EQ(actual.labelledColumns, expected.labelledColumns);
        EXPECT_EQ(actual.labelledYAxisTitle, expected.labelledYAxisTitle);
        EXPECT_EQ(actual.connection, expected.connection);
        EXPECT_EQ(actual.pointMarkers, expected.pointMarkers);
        EXPECT_EQ(actual.background, expected.background);
    }

    QStringList itemTexts(const ui::CaptureLayout &layout) {
        QStringList texts;
        for (const ui::CaptureItem &item: layout.items) {
            if (const auto *text = std::get_if<ui::CaptureTextItem>(&item)) texts << text->text;
        }
        return texts;
    }

    QRectF graphItemRect(const ui::CaptureLayout &layout) {
        for (const ui::CaptureItem &item: layout.items) {
            if (const auto *graph = std::get_if<ui::CaptureGraphItem>(&item)) return graph->rect;
        }
        return {};
    }

    ui::CaptureLayout layoutAtReference(const ui::GraphCaptureSnapshot &snapshot) {
        const auto composition = ui::provisionalCaptureComposition();
        const QSizeF size = ui::graphReferenceSize(snapshot, composition);
        auto result = ui::layoutGraphCapture(snapshot, size, composition);
        if (auto *layout = std::get_if<ui::CaptureLayout>(&result)) return std::move(*layout);
        ADD_FAILURE() << "the reference size was rejected";
        return {};
    }

    QString utcDay(const qint64 ms) {
        return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc()).date().toString(Qt::ISODate);
    }

    class CaptureGraphFidelity : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        QString perfFile;

        void SetUp() override {
            integration::clearSavedGraphConfiguration();
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            perfFile = env.copyFixtureIntoPerformances("1wall6targets TE.perf");
            ASSERT_FALSE(perfFile.isEmpty());
        }

        void TearDown() override {
            app.reset();
            integration::clearSavedGraphConfiguration();
        }

        void writeBenchmark() const {
            QFile out(QDir(env.benchmarksDir.path()).absoluteFilePath("auto.json"));
            ASSERT_TRUE(out.open(QIODevice::WriteOnly));
            out.write(kBenchmarkJson);
        }

        void startApp() {
            app = std::make_unique<application::App>(env.settings, std::make_shared<data::ProtoDecoder>(),
                                                     env.graphConfigurationPersistence, env.statsParser,
                                                     env.benchmarkStore, env.daySource, env.clipboardWriter,
                                                     env.pngWriter);
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000));
        }

        // Runs of the fixture scenario on consecutive days; the last gains `lastRunBonus` score.
        void seedRuns(const int count, const float lastRunBonus = 0.0F) {
            auto base = app->profileService()->getLatestRun();
            domain::UserProfile profile;
            const auto source = profile.ensureSource(env.rootPath().toStdString(), "FPSAimTrainer/performances");
            base.sources.perf = {{source, "1wall6targets TE.perf"}};
            for (int i = 0; i < count; ++i) {
                auto run = base;
                run.run_id.start_time += kDayMs * i;
                if (i == count - 1) run.stored_totals.score = base.totals().score + lastRunBonus;
                ASSERT_TRUE(profile.addRun(run));
            }
            ASSERT_EQ(tests_support::seedProfile(*app->profileService(), std::move(profile)),
                      application::ProfileImportOutcome::Succeeded);
            m_firstStartMs = base.run_id.start_time;
        }

        long long m_firstStartMs = 0;
    };

    TEST_F(CaptureGraphFidelity, RunGraph) {
        ASSERT_NO_FATAL_FAILURE(startApp());
        auto *vm = app->graphVm();
        vm->fetchData(QUrl::fromLocalFile(perfFile).toString());
        ASSERT_EQ(vm->contentState(), presentation::GraphViewModel::HasData);

        auto canvas = makeCanvas(*vm, vm->enabledSeriesIds());
        const ui::GraphScene live = canvas->scene();
        ASSERT_FALSE(live.series.isEmpty());

        const auto snapshot = ui::captureGraph(ui::CaptureTarget::Run, *canvas, ui::runCaptureContext(*vm),
                                               testAppearance());
        ASSERT_TRUE(snapshot.has_value());
        ASSERT_LE(ui::captureSourcePoints(*snapshot), kPaintBudget);
        expectSameScene(snapshot->scene, live);
        EXPECT_FALSE(snapshot->scene.selectedBand.has_value());

        const ui::CaptureLayout layout = layoutAtReference(*snapshot);
        const QStringList texts = itemTexts(layout);
        ASSERT_FALSE(vm->scenarioTitle().isEmpty());
        EXPECT_TRUE(texts.contains(vm->scenarioTitle()));
        EXPECT_TRUE(texts.contains(snapshot->context.xTitle));
        for (const auto &series: live.series) EXPECT_TRUE(texts.contains(series.name)) << series.name.toStdString();

        const auto pixels = ui::captureOutputPixels(layout.size, 200);
        ASSERT_TRUE(pixels.has_value());
        const auto image = ui::renderCapture(layout, *pixels);
        ASSERT_TRUE(image.has_value());
        EXPECT_EQ(image->width(), pixels->width());
        EXPECT_EQ(image->height(), pixels->height());

        const QRect plot = QRectF(graphItemRect(layout).topLeft() * 2.0, graphItemRect(layout).size() * 2.0)
                .toAlignedRect().intersected(QRect(0, 0, image->width(), image->height()));
        const QRgb seriesRgb = live.series.front().color.rgb();
        int seriesPixels = 0;
        for (int y = plot.top(); y <= plot.bottom(); ++y) {
            for (int x = plot.left(); x <= plot.right(); ++x) {
                if ((image->pixels()[static_cast<std::size_t>(y) * image->width() + x] & 0xffffffU) ==
                    (seriesRgb & 0xffffffU))
                    ++seriesPixels;
            }
        }
        EXPECT_GT(seriesPixels, 0);
    }

    TEST_F(CaptureGraphFidelity, ScenarioHistoryExcludesTheSelectedRunBand) {
        ASSERT_NO_FATAL_FAILURE(startApp());
        ASSERT_NO_FATAL_FAILURE(seedRuns(5));
        auto *vm = app->completionHistoryVm();
        ASSERT_TRUE(QTest::qWaitFor([&] { return vm->runCount() == 5; }, 5000));

        auto canvas = makeCanvas(*vm, QVariantList{presentation::CompletionHistoryViewModel::Score});
        canvas->setMarkedX(3.0);
        const ui::GraphScene live = canvas->scene();
        ASSERT_TRUE(live.selectedBand.has_value());

        const auto snapshot = ui::captureGraph(ui::CaptureTarget::ScenarioHistory, *canvas,
                                               ui::scenarioHistoryCaptureContext(*vm), testAppearance());
        ASSERT_TRUE(snapshot.has_value());
        EXPECT_FALSE(snapshot->scene.selectedBand.has_value());

        const QRectF bounds(0, 0, 640, 320);
        const auto paint = [&](const ui::GraphScene &scene) {
            QImage image(bounds.size().toSize(), QImage::Format_ARGB32);
            image.fill(scene.background);
            QPainter painter(&image);
            ui::paintGraphScene(painter, scene, bounds, ui::AxisPainter::Style{});
            return image;
        };
        const QImage captured = paint(snapshot->scene);
        const QImage liveImage = paint(live);
        const QRectF plot = ui::graphPlotRect(bounds, live.labelledYAxis, live.xAxis);
        const qreal bandX = plot.left() + live.xAxis.normalizedPosition(live.selectedBand->x) * plot.width();

        int differing = 0;
        for (int y = 0; y < captured.height(); ++y) {
            for (int x = 0; x < captured.width(); ++x) {
                if (captured.pixel(x, y) == liveImage.pixel(x, y)) continue;
                ++differing;
                EXPECT_LE(std::abs(x - bandX), 6.0) << "differs outside the band at " << x << "," << y;
            }
        }
        EXPECT_GT(differing, 0);

        const QStringList texts = itemTexts(layoutAtReference(*snapshot));
        const auto span = vm->visibleRunSpan();
        const auto times = vm->visibleStartTimesMs();
        ASSERT_TRUE(span.has_value());
        ASSERT_TRUE(times.has_value());
        const QString range = texts.filter("Run").join(" ");
        EXPECT_TRUE(range.contains(QString::number(span->first)));
        EXPECT_TRUE(range.contains(QString::number(span->second)));
        EXPECT_TRUE(range.contains(QDateTime::fromMSecsSinceEpoch(times->first).date().toString(Qt::ISODate)));
        EXPECT_TRUE(range.contains(QDateTime::fromMSecsSinceEpoch(times->second).date().toString(Qt::ISODate)));
    }

    TEST_F(CaptureGraphFidelity, DailyPlaytime) {
        ASSERT_NO_FATAL_FAILURE(startApp());
        ASSERT_NO_FATAL_FAILURE(seedRuns(5));
        const auto lastDay = std::chrono::sys_days{std::chrono::floor<std::chrono::days>(
            std::chrono::sys_time<std::chrono::milliseconds>{
                std::chrono::milliseconds{m_firstStartMs + 4 * kDayMs}})};
        env.daySource->setToday(lastDay);

        auto *vm = app->playtimeVm();
        ASSERT_TRUE(QTest::qWaitFor([&] { return vm->hasData(); }, 5000));
        auto canvas = makeCanvas(*vm, QVariantList{presentation::PlaytimeGraphViewModel::Playtime});

        const auto snapshot = ui::captureGraph(ui::CaptureTarget::DailyPlaytime, *canvas,
                                               ui::dailyPlaytimeCaptureContext(*vm), testAppearance());
        ASSERT_TRUE(snapshot.has_value());
        const auto range = vm->xAxis().calendarRangeMs();
        ASSERT_TRUE(range.has_value());
        EXPECT_TRUE(snapshot->context.rangeText.contains(utcDay(range->first)));
        EXPECT_TRUE(snapshot->context.rangeText.contains(utcDay(range->second)));
        EXPECT_EQ(snapshot->context.title, vm->title());
        EXPECT_TRUE(itemTexts(layoutAtReference(*snapshot)).contains(vm->title()));
    }

    TEST_F(CaptureGraphFidelity, BenchmarkRankWithTierReferences) {
        ASSERT_NO_FATAL_FAILURE(writeBenchmark());
        ASSERT_NO_FATAL_FAILURE(startApp());
        ASSERT_NO_FATAL_FAILURE(seedRuns(2, 50.0F));

        auto *tracking = app->benchmarkTrackingVm();
        tracking->selectBenchmark("auto-1");
        ASSERT_TRUE(QTest::qWaitFor([&] { return tracking->state() == "trackable"; }, 3000))
            << tracking->state().toStdString();
        auto *rank = tracking->rankHistory();
        ASSERT_TRUE(QTest::qWaitFor([&] { return rank->hasData(); }, 3000));

        auto canvas = makeCanvas(*rank, QVariantList{presentation::BenchmarkHistoryViewModel::Value});
        const auto snapshot = ui::captureGraph(ui::CaptureTarget::BenchmarkRank, *canvas,
                                               ui::benchmarkHistoryCaptureContext(tracking->selectedName(), *rank),
                                               testAppearance());
        ASSERT_TRUE(snapshot.has_value());
        EXPECT_TRUE(snapshot->keepEveryYLabel);
        ASSERT_FALSE(snapshot->scene.series.isEmpty());
        const auto vmSeries = rank->series({presentation::BenchmarkHistoryViewModel::Value});
        ASSERT_EQ(vmSeries.size(), 1);
        const auto &references = snapshot->scene.series.front().references;
        ASSERT_FALSE(references.isEmpty());
        ASSERT_EQ(references.size(), vmSeries.front()->referenceLines.size());
        for (qsizetype i = 0; i < references.size(); ++i) {
            EXPECT_EQ(references.at(i).value, vmSeries.front()->referenceLines.at(i).value);
            EXPECT_EQ(references.at(i).color, vmSeries.front()->referenceLines.at(i).color);
        }
        ASSERT_TRUE(snapshot->scene.labelledYAxis.has_value());

        const ui::CaptureLayout layout = layoutAtReference(*snapshot);
        const QRectF graph = graphItemRect(layout);
        ASSERT_FALSE(graph.isEmpty());
        ui::AxisPainter::Style tickStyle;
        tickStyle.font = ui::captureFont(snapshot->appearance.font, ui::CaptureTextRole::Tick);
        const auto &yAxis = *snapshot->scene.labelledYAxis;
        const QRectF plot = ui::graphPlotRect(graph, yAxis, snapshot->scene.xAxis, tickStyle);
        const auto placements = ui::AxisPainter::layoutLabels(
            plot, graph, ui::AxisPainter::Orientation::Vertical, yAxis.min, yAxis.max, yAxis.ticks,
            [&yAxis](const qreal tick) { return yAxis.labelFor(tick); }, tickStyle);
        EXPECT_EQ(placements.size(), yAxis.ticks.size());

        const QStringList texts = itemTexts(layout);
        EXPECT_TRUE(texts.contains(tracking->selectedName()));
        EXPECT_TRUE(texts.contains(rank->metricName()));

        const auto pixels = ui::captureOutputPixels(layout.size, 100);
        ASSERT_TRUE(pixels.has_value());
        EXPECT_TRUE(ui::renderCapture(layout, *pixels).has_value());
    }

    TEST_F(CaptureGraphFidelity, RepeatingACaptureLeavesTheSourceUntouched) {
        ASSERT_NO_FATAL_FAILURE(startApp());
        ASSERT_NO_FATAL_FAILURE(seedRuns(3));
        auto *vm = app->graphVm();
        vm->fetchData(QUrl::fromLocalFile(perfFile).toString());
        ASSERT_EQ(vm->contentState(), presentation::GraphViewModel::HasData);
        auto canvas = makeCanvas(*vm, vm->enabledSeriesIds());

        const ui::GraphScene before = canvas->scene();
        const auto configurationBefore = app->graphConfigurationUseCase()->current().configuration;
        const auto readProfile = [&] {
            QFile file(env.profileStorePath());
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };
        const QByteArray profileBefore = readProfile();

        for (int i = 0; i < 2; ++i) {
            const auto snapshot = ui::captureGraph(ui::CaptureTarget::Run, *canvas, ui::runCaptureContext(*vm),
                                                   testAppearance());
            ASSERT_TRUE(snapshot.has_value());
            ASSERT_NE(ui::renderCapture(layoutAtReference(*snapshot), QSize(320, 200)), std::nullopt);
        }

        expectSameScene(canvas->scene(), before);
        EXPECT_EQ(*app->graphConfigurationUseCase()->current().configuration, *configurationBefore);
        EXPECT_EQ(readProfile(), profileBefore);
    }
}
