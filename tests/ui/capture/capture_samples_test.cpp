// Writes the sample PNGs for review, and checks what only the real fonts can show. Nothing here runs in the routine
// suite except the builder checks. Without KSV_CAPTURE_SAMPLES_DIR the sample generators skip before touching the
// disk. They need the "windows" platform plugin to resolve the app's system font.

#include <gtest/gtest.h>

#include <QDir>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "capture/capture_appearance.h"
#include "capture/capture_fonts.h"
#include "capture/capture_renderer.h"
#include "capture/capture_sizing.h"
#include "capture/capture_sources.h"
#include "capture/graph_capture_layout.h"
#include "capture/table_capture_layout.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/benchmarks/tracking/benchmark_history_vm.h"
#include "presentation/graphs/common/axes/value_axis.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"
#include "table_capture_fixtures.h"

#include "../benchmarks/tracking/fake_benchmark_tracking_use_case.h"

namespace ksv::ui {
    namespace {
        using namespace ksv::application;
        using namespace ksv::domain;
        using presentation::BenchmarkTrackingViewModel;
        using presentation::SeriesModel;
        using presentation::ValueAxis;
        using tests_ui::FakeBenchmarkTrackingUseCase;

        constexpr auto kThemeUri = "KsvCaptureSamples";
        constexpr int kResolutions[] = {100, 200};

        QObject *loadTheme(QQmlEngine &engine) {
            const QString path = QStringLiteral(KSV_SOURCE_DIR "/src/ui/qml/common/Theme.qml");
            qmlRegisterSingletonType(QUrl::fromLocalFile(path), kThemeUri, 1, 0, "Theme");
            return engine.singletonInstance<QObject *>(kThemeUri, "Theme");
        }

        class DenseVm : public presentation::GraphViewModelBase {
        public:
            DenseVm() {
                m_xAxis.setRange(0.0, 500.0);
                static const QColor colors[] = {QColor("#42d9e8"), QColor("#e67828"), QColor("#8fd45a"),
                                                QColor("#d96bd0")};
                for (int s = 0; s < 4; ++s) {
                    auto *series = new SeriesModel(this);
                    series->setColumn(s + 1);
                    series->setId(QStringLiteral("dense-%1").arg(s + 1));
                    series->setName(QStringLiteral("Series %1").arg(s + 1));
                    series->setColor(colors[s]);
                    QList<QPointF> points;
                    for (int i = 0; i < 500; ++i) {
                        const double x = i;
                        points.append(QPointF(x, 50.0 + 10.0 * s + 30.0 * std::sin(x / (12.0 + 5.0 * s)) +
                                                     8.0 * std::sin(x / 3.0 + s)));
                    }
                    series->setData(points);
                    m_series.append(series);
                }
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

        std::unique_ptr<GraphCanvas> makeCanvas(presentation::GraphViewModelBase &vm, const QVariantList &columns) {
            auto canvas = std::make_unique<GraphCanvas>();
            canvas->setWidth(800);
            canvas->setHeight(400);
            canvas->setGraphVm(&vm);
            canvas->setVisibleColumns(columns);
            canvas->setConnectionMode(static_cast<GraphCanvas::ConnectionMode>(vm.connectionMode()));
            canvas->setPointMarkers(vm.pointMarkers());
            return canvas;
        }

        std::vector<Tier> sampleTiers(const int count) {
            static const char *const names[] = {"Iron", "Bronze", "Silver", "Gold", "Platinum",
                                                "Diamond", "Jade", "Master", "Grandmaster", "Nova"};
            std::vector<Tier> tiers;
            for (int i = 0; i < count; ++i) {
                const QColor color = QColor::fromHsv((i * 37 + 10) % 360, 190, 230);
                tiers.push_back(Tier{TierId{"t" + std::to_string(i + 1)}, names[i],
                                     BenchmarkColor{static_cast<std::uint8_t>(color.red()),
                                                    static_cast<std::uint8_t>(color.green()),
                                                    static_cast<std::uint8_t>(color.blue())}});
            }
            return tiers;
        }

        struct TableSpec {
            int rows;
            int tierCount;
            bool incomplete;
        };

        // Eight-row slots fill Tracking's Smooth and Reactive subcategories, Clicking's direct entries,
        // and Switching's Static and Evasive subcategories. Static keeps Switching from mixing direct entries
        // with subcategories, which validation rejects as MixedCategoryContent.
        BenchmarkWorkspaceSnapshot sampleTableSnapshot(const TableSpec &spec) {
            tests_ui::BenchmarkFixture fixture;
            auto &definition = fixture.definition;
            definition.id = BenchmarkId{"sample"};
            definition.name = spec.incomplete ? "Incomplete Sample Benchmark" : "Overflowing Sample Benchmark";
            definition.tiers = sampleTiers(spec.tierCount);
            const auto &tiers = definition.tiers;
            const std::size_t tierCount = tiers.size();

            struct Slot {
                GroupId id;
                std::string name;
                BenchmarkColor color;
                std::vector<ScenarioEntry> entries;
            };
            std::vector<Slot> groupSlots = {
                {GroupId{"sc1"}, "Smooth", {0, 200, 0}, {}},
                {GroupId{"sc2"}, "Reactive", {0, 160, 255}, {}},
                {GroupId{"c2"}, "Clicking", {255, 80, 80}, {}},
                {GroupId{"sc4"}, "Static", {200, 160, 0}, {}},
                {GroupId{"sc3"}, "Evasive", {180, 80, 220}, {}},
            };

            for (int i = 0; i < spec.rows; ++i) {
                Slot &slot = groupSlots[static_cast<std::size_t>(std::min(i / 8, 4))];
                const std::string id = "e" + std::to_string(i);
                const double base = 1000.0 + i * 7.0;

                ScenarioEntry entry{ScenarioEntryId{id}, "Sample Drill Number " + std::to_string(i + 1),
                                    std::vector<std::string>{"hash" + std::to_string(i)}, {}};
                std::vector<double> scores;
                for (std::size_t t = 0; t < tierCount; ++t) {
                    scores.push_back(base * (1.0 + 0.25 * static_cast<double>(t)));
                    const bool dropped = spec.incomplete && ((i % 4 == 0 && t == 2) || (i == 1 && t >= 4));
                    if (!dropped) entry.thresholds.push_back(Threshold{tiers[t].id, scores[t]});
                }

                const int reached = i % static_cast<int>(tierCount + 1);
                const bool unplayed = i % 13 == 5;
                const bool unmapped = i % 17 == 9;
                std::optional<double> personalBest;
                if (!unplayed && !unmapped) {
                    if (reached == 0) personalBest = scores[0] * 0.4;
                    else if (reached >= static_cast<int>(tierCount)) personalBest = scores.back() * 1.1;
                    else {
                        const auto r = static_cast<std::size_t>(reached);
                        personalBest = scores[r - 1] + 0.5 * (scores[r] - scores[r - 1]);
                    }
                }

                fixture.resolutions.push_back(unmapped ? tests_support::unresolvedEntry(id)
                                                       : tests_support::resolvedEntry(id));
                if (personalBest) {
                    const auto day = tests_ui::ymd(2024, 3, 4) + std::chrono::days{i * 3};
                    const auto pb = static_cast<float>(std::round(*personalBest * 8) / 8);
                    const auto &hash = entry.hashes.front();
                    fixture.runs.push_back(tests_ui::runOn(day, hash, pb / 2, 60));
                    fixture.runs.push_back(tests_ui::runOn(day + std::chrono::days{1}, hash, pb * 0.75F, 60));
                    fixture.runs.push_back(tests_ui::runOn(day + std::chrono::days{2}, hash, pb, 60));
                }
                slot.entries.push_back(std::move(entry));
            }

            const auto addSubcategory = [](Category &category, Slot &slot) {
                if (!slot.entries.empty())
                    category.subcategories.push_back({slot.id, slot.name, slot.color, std::move(slot.entries)});
            };
            Category tracking{GroupId{"c1"}, "Tracking", {0, 128, 255}, {}, {}};
            addSubcategory(tracking, groupSlots[0]);
            addSubcategory(tracking, groupSlots[1]);
            if (!tracking.subcategories.empty()) definition.categories.push_back(std::move(tracking));

            auto &clicking = groupSlots[2];
            if (!clicking.entries.empty())
                definition.categories.push_back({clicking.id, clicking.name, clicking.color, std::move(clicking.entries), {}});

            Category switching{GroupId{"c3"}, "Switching", {200, 160, 0}, {}, {}};
            addSubcategory(switching, groupSlots[3]);
            addSubcategory(switching, groupSlots[4]);
            if (!switching.subcategories.empty()) definition.categories.push_back(std::move(switching));
            return tests_ui::snapshotFor(fixture);
        }

        BenchmarkWorkspaceSnapshot sampleRankSnapshot() {
            auto fixture = tests_ui::makeUniformFixture(8, {});
            fixture.definition.tiers = sampleTiers(8);
            for (int day = 0; day < 14; ++day) {
                fixture.runs.push_back(tests_ui::runOn(
                    tests_ui::ymd(2024, 3, 1) + std::chrono::days{day * 2}, "u", 0.5F * (day + 1), 60));
            }
            auto snapshot = tests_ui::snapshotFor(fixture);
            snapshot.visiblePlaytime = {{tests_ui::ymd(2024, 3, 4), 1800.0},
                                        {tests_ui::ymd(2024, 4, 20), 900.0}};
            return snapshot;
        }

        void writeSamples(const QString &dir, const QString &name, const CaptureLayout &layout) {
            if (const std::optional<QRectF> plot = graphCapturePlot(layout)) {
                ::testing::Test::RecordProperty(QStringLiteral("%1-plotAspect").arg(name).toStdString(),
                                                QString::number(plot->width() / plot->height(), 'f', 3).toStdString());
            }
            for (const int percent: kResolutions) {
                const std::optional<QSize> pixels = captureOutputPixels(layout.size, percent);
                ASSERT_TRUE(pixels.has_value()) << name.toStdString();
                const std::optional<RasterImage> raster = renderCapture(layout, *pixels);
                ASSERT_TRUE(raster.has_value()) << name.toStdString();
                ASSERT_EQ(QSize(raster->width(), raster->height()), *pixels);

                const QImage image(reinterpret_cast<const uchar *>(raster->pixels()), raster->width(),
                                   raster->height(), static_cast<qsizetype>(raster->stride()),
                                   QImage::Format_RGB32);
                const QString path = QDir(dir).absoluteFilePath(QStringLiteral("%1-%2.png").arg(name).arg(percent));
                ASSERT_TRUE(image.save(path, "PNG")) << path.toStdString();

                const QImage decoded(path);
                ASSERT_FALSE(decoded.isNull()) << path.toStdString();
                EXPECT_EQ(decoded.size(), *pixels) << path.toStdString();
                ::testing::Test::RecordProperty(
                    QStringLiteral("%1-%2").arg(name).arg(percent).toStdString(),
                    std::to_string(pixels->width()) + "x" + std::to_string(pixels->height()));
            }
        }

        CaptureLayout layoutGraph(const GraphCaptureSnapshot &snapshot) {
            const CaptureComposition composition = standardCaptureComposition();
            const QSizeF size = graphReferenceSize(snapshot, composition);
            auto result = layoutGraphCapture(snapshot, size, composition);
            if (auto *layout = std::get_if<CaptureLayout>(&result)) return std::move(*layout);
            ADD_FAILURE() << "the reference size was rejected";
            return {};
        }
    }

    TEST(CaptureSamples, GeneratesTheEightSamples) {
        const QString dir = qEnvironmentVariable("KSV_CAPTURE_SAMPLES_DIR");
        if (dir.isEmpty()) GTEST_SKIP() << "KSV_CAPTURE_SAMPLES_DIR is not set";

        ASSERT_EQ(QGuiApplication::platformName(), QStringLiteral("windows"))
            << "the samples must use the real platform's fonts";
        ASSERT_TRUE(QDir().mkpath(dir));

        QQmlEngine engine;
        QObject *theme = loadTheme(engine);
        ASSERT_NE(theme, nullptr);

        QQmlComponent labelComponent(&engine);
        labelComponent.setData("import QtQuick.Controls\nLabel {}", QUrl());
        std::unique_ptr<QObject> label(labelComponent.create());
        ASSERT_TRUE(label) << labelComponent.errorString().toStdString();
        const QFont labelFont = label->property("font").value<QFont>();

        const CaptureAppearance appearance = captureAppearanceFromTheme(*theme, labelFont);
        const QString expectedFamily = QFontInfo(labelFont).family();
        for (const CaptureTextRole role: {CaptureTextRole::Title, CaptureTextRole::Context, CaptureTextRole::Body,
                                          CaptureTextRole::BodyBold, CaptureTextRole::Small,
                                          CaptureTextRole::SmallBold, CaptureTextRole::Tick}) {
            EXPECT_EQ(QFontInfo(captureFont(appearance.font, role)).family(), expectedFamily);
        }
        const QString boldSample = QStringLiteral("The quick brown fox jumps over the lazy dog 0123456789");
        EXPECT_GT(QFontMetricsF(captureFont(appearance.font, CaptureTextRole::BodyBold)).horizontalAdvance(boldSample),
                  QFontMetricsF(captureFont(appearance.font, CaptureTextRole::Body)).horizontalAdvance(boldSample));
        RecordProperty("fontFamily", expectedFamily.toStdString());
        RecordProperty("fontPixelSize", appearance.font.pixelSize);
        RecordProperty("platform", QGuiApplication::platformName().toStdString());
        if (HasFailure()) return;

        DenseVm denseVm;
        auto denseCanvas = makeCanvas(denseVm, QVariantList{1, 2, 3, 4});
        const auto dense = captureGraph(CaptureTarget::Run, *denseCanvas,
                                        GraphCaptureContext{.title = QStringLiteral("Dense sample run"),
                                                            .graphType = QStringLiteral("Run graph"),
                                                            .rangeText = QStringLiteral("0 to 500 s"),
                                                            .xTitle = QStringLiteral("Time (s)")},
                                        appearance);
        ASSERT_TRUE(dense.has_value());
        ASSERT_NO_FATAL_FAILURE(writeSamples(dir, QStringLiteral("dense-graph"), layoutGraph(*dense)));

        auto rankFake = std::make_shared<FakeBenchmarkTrackingUseCase>();
        rankFake->snap = sampleRankSnapshot();
        BenchmarkTrackingViewModel rankTracking{rankFake};
        auto *rank = rankTracking.rankHistory();
        auto rankCanvas = makeCanvas(*rank, QVariantList{presentation::BenchmarkHistoryViewModel::Value});
        const auto rankSnapshot = captureGraph(CaptureTarget::BenchmarkRank, *rankCanvas,
                                               benchmarkHistoryCaptureContext(rankTracking.selectedName(), *rank),
                                               appearance);
        ASSERT_TRUE(rankSnapshot.has_value());
        ASSERT_NO_FATAL_FAILURE(writeSamples(dir, QStringLiteral("rank-graph"), layoutGraph(*rankSnapshot)));

        const std::pair<TableSpec, const char *> tables[] = {{{40, 10, false}, "overflow-table"},
                                                             {{24, 6, true}, "incomplete-table"}};
        for (const auto &[spec, name]: tables) {
            auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
            fake->snap = sampleTableSnapshot(spec);
            BenchmarkTrackingViewModel vm{fake};
            ASSERT_EQ(vm.state(), spec.incomplete ? "incomplete" : "trackable");
            const auto table = captureTable(vm, appearance);
            ASSERT_TRUE(table.has_value()) << name;
            ASSERT_NO_FATAL_FAILURE(writeSamples(dir, QString::fromLatin1(name), layoutTableCapture(*table, true)));
        }
    }

    // The room past this fill holds the value at its plain width but not at its bold one, so a layout that measured
    // the wrong face would send the value past the fill and off the end of the bar.
    TEST(CaptureSamples, TheBoldValueIsMeasuredInItsOwnFace) {
        if (qEnvironmentVariable("KSV_CAPTURE_SAMPLES_DIR").isEmpty()) {
            GTEST_SKIP() << "KSV_CAPTURE_SAMPLES_DIR is not set";
        }
        ASSERT_EQ(QGuiApplication::platformName(), QStringLiteral("windows"))
            << "the offscreen stack has no bold face";

        TableCaptureSnapshot snapshot = tableFixtureSnapshot(
            {QStringLiteral("Drill")}, {tableFixtureGroup(QStringLiteral("Everything"), 0, 1, tableFixtureTint(0))}, 1);
        presentation::ProgressGridScenarioRow &row = snapshot.grid.rows.at(0);
        row.personalBestText = QStringLiteral("8,888,888,888,888,888");

        const auto advance = [&](const CaptureTextRole role) {
            return QFontMetricsF(captureFont(snapshot.appearance.font, role)).horizontalAdvance(row.personalBestText);
        };
        const qreal plainWidth = advance(CaptureTextRole::Small);
        const qreal boldWidth = advance(CaptureTextRole::SmallBold);
        ASSERT_GT(boldWidth, plainWidth + 2.0);

        struct Bar {
            QRectF value;
            CaptureTextRole valueRole = CaptureTextRole::Small;
            QRectF fill;
            QRectF track;
        };
        const auto measure = [&](const double fill) {
            row.fills = {fill};
            const CaptureLayout layout = layoutTableCapture(snapshot, false);
            const QColor barColor(snapshot.grid.tiers.at(0).barColor);
            const QRgb textColor = snapshot.appearance.text.rgb();
            Bar bar;
            for (const CaptureItem &item : layout.items) {
                if (const auto *text = std::get_if<CaptureTextItem>(&item); text && text->text == row.personalBestText) {
                    bar.value = text->rect;
                    bar.valueRole = text->role;
                }
                if (const auto *rect = std::get_if<CaptureRectItem>(&item)) {
                    if (rect->fill == barColor && rect->radius > 0.0) bar.fill = rect->rect;
                    if (rect->fill.rgb() == textColor && rect->fill.alpha() < 255) bar.track = rect->rect;
                }
            }
            return bar;
        };

        const Bar probe = measure(0.1);
        ASSERT_FALSE(probe.value.isEmpty());
        ASSERT_FALSE(probe.fill.isEmpty());
        ASSERT_FALSE(probe.track.isEmpty());
        const qreal gap = probe.value.left() - probe.fill.right();
        const qreal roomPastFill = gap + (plainWidth + boldWidth) / 2.0;

        const Bar bar = measure((probe.track.width() - roomPastFill) / probe.track.width());

        EXPECT_GE(bar.value.width(), boldWidth);
        EXPECT_DOUBLE_EQ(advance(bar.valueRole), boldWidth);
        EXPECT_LE(bar.value.right(), bar.fill.right());
        EXPECT_GE(bar.value.left(), bar.track.left());
    }

    TEST(CaptureSamples, SampleBuildersEvaluateToTheIntendedStates) {
        auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
        fake->snap = sampleTableSnapshot({40, 10, false});
        BenchmarkTrackingViewModel vm{fake};
        EXPECT_EQ(vm.state(), "trackable");
        EXPECT_EQ(vm.progressGrid()->rows().size(), 40);
        EXPECT_TRUE(vm.summaryAvailable());

        fake->snap = sampleTableSnapshot({24, 6, true});
        fake->publish();
        EXPECT_EQ(vm.state(), "incomplete");
        EXPECT_EQ(vm.progressGrid()->rows().size(), 24);
        EXPECT_FALSE(vm.summaryAvailable());
        EXPECT_EQ(vm.attainedRank(), "Unavailable");
        EXPECT_EQ(vm.averageRank(), "Unavailable");

        fake->snap = sampleRankSnapshot();
        fake->publish();
        EXPECT_EQ(vm.state(), "trackable");
        const auto rank = vm.rankHistory()->series({1});
        ASSERT_EQ(rank.size(), 1);
        EXPECT_EQ(rank.front()->points.size(), 14);
        const auto playtime = vm.playtimeHistory()->series({1});
        ASSERT_EQ(playtime.size(), 1);
        EXPECT_EQ(playtime.front()->points.size(), 2);
    }
}
