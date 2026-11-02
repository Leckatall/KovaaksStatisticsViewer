// Writes the sample PNGs for review. Nothing here runs in the routine suite: without
// KSV_CAPTURE_SAMPLES_DIR the case skips before touching the disk, and it refuses to run on any platform other
// than the real "windows" one, whose system font is what the app resolves.

#include <gtest/gtest.h>

#include <QDir>
#include <QFontInfo>
#include <QGuiApplication>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

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
#include "presentation/graphs/common/axes/value_axis.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"

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

        // Every eighth scenario starts a new group: two subcategories of "Tracking", the direct scenarios of
        // "Clicking", then "Switching" with direct scenarios and a subcategory.
        BenchmarkWorkspaceSnapshot sampleTableSnapshot(const TableSpec &spec) {
            BenchmarkWorkspaceSnapshot snapshot = tests_ui::makeBreakdownSnapshot();
            const std::vector<Tier> tiers = sampleTiers(spec.tierCount);
            const std::size_t tierCount = tiers.size();

            Benchmark definition;
            definition.id = BenchmarkId{"sample"};
            definition.name = spec.incomplete ? "Incomplete Sample Benchmark" : "Overflowing Sample Benchmark";
            definition.tiers = tiers;

            BenchmarkProjection projection;
            projection.benchmarkId = definition.id;
            projection.tiers = tiers;

            struct Slot {
                GroupId id;
                std::string name;
                BenchmarkColor color;
                int tier;
                std::vector<ScenarioEntry> entries;
                std::vector<ScenarioEntryId> ids;
            };
            std::vector<Slot> groupSlots = {
                {GroupId{"sc1"}, "Smooth", {0, 200, 0}, 3, {}, {}},
                {GroupId{"sc2"}, "Reactive", {0, 160, 255}, 2, {}, {}},
                {GroupId{"c2"}, "Clicking", {255, 80, 80}, 1, {}, {}},
                {GroupId{"c3"}, "Switching", {200, 160, 0}, 4, {}, {}},
                {GroupId{"sc3"}, "Evasive", {180, 80, 220}, 5, {}, {}},
            };

            for (int i = 0; i < spec.rows; ++i) {
                Slot &slot = groupSlots[static_cast<std::size_t>(std::min(i / 8, 4))];
                const std::string id = "e" + std::to_string(i);
                const double base = 1000.0 + i * 7.0;

                ScenarioEntry entry{ScenarioEntryId{id}, "Sample Drill Number " + std::to_string(i + 1),
                                    std::string{"hash" + std::to_string(i)}, {}};
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

                ScenarioProjection scenario;
                scenario.entryId = entry.id;
                scenario.name = entry.name;
                scenario.matchState = unmapped ? ScenarioMatchState::Unresolved : ScenarioMatchState::Resolved;
                scenario.personalBest = personalBest;
                if (personalBest) {
                    scenario.recentAverage = *personalBest * (i % 3 == 0 ? 0.85 : 0.97);
                    scenario.recentSampleCount = 4;
                    scenario.runCount = 10 + i;
                    scenario.personalBestTime = std::chrono::sys_seconds{std::chrono::seconds{1'700'000'000 + i * 86'400}};
                    if (reached >= 1) scenario.attainedTier = tiers[static_cast<std::size_t>(std::min<int>(reached, static_cast<int>(tierCount)) - 1)].id;
                    if (reached < static_cast<int>(tierCount)) {
                        const auto r = static_cast<std::size_t>(reached);
                        scenario.nextThreshold = Threshold{tiers[r].id, scores[r]};
                    }
                }
                if (!spec.incomplete && i % 6 == 2 && personalBest) {
                    projection.nextTierBlockers.push_back(
                        RankBlocker{entry.id, slot.id, scores[tierCount / 2], personalBest});
                }
                projection.scenarios.push_back(scenario);
                slot.entries.push_back(entry);
                slot.ids.push_back(entry.id);
            }

            const auto groupOf = [&](const Slot &slot) {
                return GroupProjection{slot.id, slot.name, tiers[static_cast<std::size_t>(slot.tier) % tierCount].id,
                                       slot.ids, {}};
            };
            const auto subOf = [](const Slot &slot) {
                return Subcategory{slot.id, slot.name, slot.color, slot.entries};
            };

            Category tracking{GroupId{"c1"}, "Tracking", {0, 128, 255}, {}, {}};
            GroupProjection trackingProjection{GroupId{"c1"}, "Tracking", std::nullopt, {}, {}};
            for (const Slot *slot: {&groupSlots[0], &groupSlots[1]}) {
                if (slot->entries.empty()) continue;
                tracking.subcategories.push_back(subOf(*slot));
                trackingProjection.subgroups.push_back(groupOf(*slot));
            }
            if (!tracking.subcategories.empty()) {
                definition.categories.push_back(tracking);
                projection.categories.push_back(trackingProjection);
            }
            if (!groupSlots[2].entries.empty()) {
                definition.categories.push_back(
                    Category{groupSlots[2].id, groupSlots[2].name, groupSlots[2].color, groupSlots[2].entries, {}});
                projection.categories.push_back(groupOf(groupSlots[2]));
            }
            if (!groupSlots[3].entries.empty() || !groupSlots[4].entries.empty()) {
                Category switching{groupSlots[3].id, groupSlots[3].name, groupSlots[3].color, groupSlots[3].entries, {}};
                GroupProjection switchingProjection = groupOf(groupSlots[3]);
                if (!groupSlots[4].entries.empty()) {
                    switching.subcategories.push_back(subOf(groupSlots[4]));
                    switchingProjection.subgroups.push_back(groupOf(groupSlots[4]));
                }
                definition.categories.push_back(switching);
                projection.categories.push_back(switchingProjection);
            }

            CompletenessResult completeness{Completeness::Trackable, {}};
            projection.completeness = Completeness::Trackable;
            if (spec.incomplete) {
                completeness = CompletenessResult{
                    Completeness::Incomplete, {BenchmarkIssue{BenchmarkIssueCode::MissingThreshold, std::monostate{}}}};
                projection.completeness = Completeness::Incomplete;
            } else {
                projection.attainedRank = tiers[tierCount / 2].id;
                projection.completedRank = tiers[tierCount / 2 - 1].id;
                projection.nextTier = tiers[tierCount / 2 + 1].id;
                projection.averageRank = static_cast<double>(tierCount) / 2.0 + 0.4;
                projection.scenariosAtNextTier = 3;
            }
            projection.totalPlaytimeSeconds = 86'400.0;

            snapshot.selectedLoaded = data::LoadedBenchmark{definition, completeness};
            snapshot.projection = projection;
            snapshot.lastKnownSelectedDisplayName = definition.name;
            snapshot.choices.front().displayName = definition.name;
            snapshot.choices.front().id = definition.id;
            snapshot.selectedId = definition.id;
            return snapshot;
        }

        BenchmarkWorkspaceSnapshot sampleRankSnapshot() {
            BenchmarkWorkspaceSnapshot snapshot = tests_ui::makeTrackableSnapshot();
            const std::vector<Tier> tiers = sampleTiers(8);
            snapshot.selectedLoaded->benchmark.tiers = tiers;
            snapshot.projection->tiers = tiers;
            snapshot.projection->averageRankHistory.clear();
            for (int day = 0; day < 14; ++day) {
                snapshot.projection->averageRankHistory.push_back(
                    {tests_ui::ymd(2024, 3, 1) + std::chrono::days{day * 2}, 0.6 + 0.45 * day});
            }
            snapshot.projection->rollingPlaytime = {{tests_ui::ymd(2024, 3, 4), 1800.0},
                                                    {tests_ui::ymd(2024, 4, 20), 900.0}};
            return snapshot;
        }

        void writeSamples(const QString &dir, const QString &name, const CaptureLayout &layout) {
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
            const CaptureComposition composition = provisionalCaptureComposition();
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
                                          CaptureTextRole::Tick}) {
            EXPECT_EQ(QFontInfo(captureFont(appearance.font, role)).family(), expectedFamily);
        }
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
}
