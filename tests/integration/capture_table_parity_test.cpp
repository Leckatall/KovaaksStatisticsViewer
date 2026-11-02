#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTest>
#include <QUrl>

#include <algorithm>
#include <memory>
#include <optional>
#include <QPointF>
#include <variant>
#include <vector>

#include "app/app.h"
#include "capture/capture_layout.h"
#include "capture/capture_sources.h"
#include "capture/table_capture_layout.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmarks/tracking/benchmark_progress_grid_model.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"

#include "integration_env.h"
#include "../ui/capture/capture_test_support.h"
#include "../ui/capture/table_capture_fixtures.h"

using namespace ksv;

namespace {
    constexpr auto kWallSixHash = "3e50391f3c3f484c10a4b8fb362ded17";
    constexpr auto kUnplayedHash = "ffffffffffffffffffffffffffffffff";
    constexpr int kTierCount = 8;
    constexpr int kRowCount = 30;
    const QString kBenchmarkId = QStringLiteral("parity-bench");

    const QStringList kTierNames{"Iron", "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Jade", "Master"};

    // The fixture row sits just past the start of its sixth chunk, so the value label crowds the labels there.
    constexpr int kCrowdedTier = 5;

    QJsonArray colorOf(const int index) {
        return QJsonArray{(index * 37) % 200 + 30, (index * 71) % 200 + 30, (index * 113) % 200 + 30, 255};
    }

    QJsonObject entry(const QString &id, const QString &name, const QJsonValue &hash, const QList<double> &scores,
                      const int skippedTier = -1) {
        QJsonArray thresholds;
        for (int tier = 0; tier < scores.size(); ++tier) {
            if (tier == skippedTier) continue;
            thresholds.append(QJsonObject{{"tierId", QStringLiteral("t%1").arg(tier)}, {"score", scores.at(tier)}});
        }
        return QJsonObject{{"id", id}, {"name", name}, {"hash", hash}, {"thresholds", thresholds}};
    }

    QList<double> ladder(const int row) {
        QList<double> scores;
        for (int tier = 0; tier < kTierCount; ++tier) scores.append((row + 1) * 1000.0 + tier * 13.0);
        return scores;
    }

    // Thresholds that put `personalBest` a few percent into the chunk of kCrowdedTier.
    QList<double> ladderAround(const double personalBest) {
        QList<double> scores;
        for (int tier = 0; tier < kTierCount; ++tier) {
            if (tier < kCrowdedTier - 1) scores.append(personalBest * (0.2 * (tier + 1)));
            else if (tier == kCrowdedTier - 1) scores.append(personalBest * 0.99);
            else scores.append(personalBest * (1.3 + 0.3 * (tier - kCrowdedTier)));
        }
        return scores;
    }

    QJsonArray scenarios(const QString &prefix, const int from, const int count, const double personalBest,
                         const bool playFirst) {
        QJsonArray out;
        for (int i = 0; i < count; ++i) {
            const int row = from + i;
            const QString id = QStringLiteral("%1%2").arg(prefix).arg(row);
            if (playFirst && i == 0) {
                out.append(entry(id, QStringLiteral("Wall Six"), kWallSixHash, ladderAround(personalBest)));
            } else if (i == 1) {
                out.append(entry(id, QStringLiteral("Unplayed Drill %1").arg(row), kUnplayedHash, ladder(row)));
            } else if (i == 2) {
                out.append(entry(id, QStringLiteral("Unmapped Drill %1").arg(row), QJsonValue::Null, ladder(row)));
            } else if (i == 3) {
                out.append(entry(id, QStringLiteral("Missing Threshold Drill %1").arg(row),
                                 QStringLiteral("%1%2").arg(QString(kUnplayedHash).left(30)).arg(row, 2, 10, QChar('0')),
                                 ladder(row), 3));
            } else {
                out.append(entry(id, QStringLiteral("Drill %1").arg(row),
                                 QStringLiteral("%1%2").arg(QString(kUnplayedHash).left(30)).arg(row, 2, 10, QChar('0')),
                                 ladder(row)));
            }
        }
        return out;
    }

    QByteArray benchmarkJson(const double personalBest) {
        QJsonArray tiers;
        for (int tier = 0; tier < kTierCount; ++tier) {
            tiers.append(QJsonObject{{"id", QStringLiteral("t%1").arg(tier)}, {"name", kTierNames.at(tier)},
                                     {"color", colorOf(tier)}});
        }

        QJsonArray uncategorized;
        for (int i = 0; i < 3; ++i) {
            const QString name = i == 2
                ? QStringLiteral("An exceedingly long uncategorised scenario name that no identity column "
                                 "should be allowed to elide or crop away in the exported table image")
                : QStringLiteral("Loose Drill %1").arg(i);
            uncategorized.append(entry(QStringLiteral("u%1").arg(i), name,
                                       QStringLiteral("%1%2").arg(QString(kUnplayedHash).left(30)).arg(90 + i),
                                       ladder(i)));
        }

        const QJsonObject tracking{
            {"id", "c-track"}, {"name", "Tracking"}, {"color", colorOf(10)}, {"scenarios", QJsonArray{}},
            {"subcategories",
             QJsonArray{
                 QJsonObject{{"id", "sc-smooth"}, {"name", "Smooth"}, {"color", colorOf(11)},
                             {"scenarios", scenarios("a", 3, 8, personalBest, true)}},
                 QJsonObject{{"id", "sc-reactive"}, {"name", "Reactive"}, {"color", colorOf(12)},
                             {"scenarios", scenarios("b", 11, 8, personalBest, false)}},
             }}};
        const QJsonObject clicking{
            {"id", "c-click"}, {"name", "Clicking"}, {"color", colorOf(13)},
            {"scenarios", scenarios("c", 19, 8, personalBest, false)}, {"subcategories", QJsonArray{}}};
        const QJsonObject switching{
            {"id", "c-switch"}, {"name", "Switching"}, {"color", colorOf(14)}, {"scenarios", QJsonArray{}},
            {"subcategories",
             QJsonArray{QJsonObject{{"id", "sc-wide"}, {"name", "Wide"}, {"color", colorOf(15)},
                                    {"scenarios", scenarios("d", 27, 3, personalBest, false)}}}}};

        return QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"id", kBenchmarkId}, {"name", "Parity Benchmark"},
                                         {"tiers", tiers}, {"uncategorized", uncategorized},
                                         {"categories", QJsonArray{tracking, clicking, switching}}})
            .toJson();
    }

    bool writeBenchmarkFile(const integration::TestEnv &env, const QByteArray &json) {
        QFile out(QDir(env.benchmarksDir.path()).absoluteFilePath("parity.json"));
        if (!out.open(QIODevice::WriteOnly)) return false;
        out.write(json);
        return true;
    }

    std::unique_ptr<application::App> startApp(integration::TestEnv &env) {
        auto app = std::make_unique<application::App>(env.settings, std::make_shared<data::ProtoDecoder>(),
                                                      env.graphConfigurationPersistence, env.statsParser,
                                                      env.benchmarkStore, env.daySource, env.clipboardWriter,
                                                      env.pngWriter);
        if (!QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000)) return nullptr;
        return app;
    }

    // The fixture perf's personal best, read through a throwaway app so the real benchmark can be written around it.
    std::optional<double> fixturePersonalBest(integration::TestEnv &env) {
        const QJsonObject probe{
            {"schemaVersion", 1}, {"id", "probe"}, {"name", "Probe"},
            {"tiers", QJsonArray{QJsonObject{{"id", "t0"}, {"name", "Iron"}, {"color", colorOf(0)}}}},
            {"uncategorized", QJsonArray{entry("p0", "Wall Six", kWallSixHash, {1.0})}},
            {"categories", QJsonArray{}}};
        if (!writeBenchmarkFile(env, QJsonDocument(probe).toJson())) return std::nullopt;
        const auto app = startApp(env);
        if (!app) return std::nullopt;
        app->benchmarkTrackingVm()->selectBenchmark(QStringLiteral("probe"));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        const auto *projection = app->benchmarkTrackingUseCase()->projection();
        if (!projection || projection->scenarios.empty()) return std::nullopt;
        return projection->scenarios.front().personalBest;
    }

    QString squashed(QString text) {
        return text.remove(QRegularExpression(QStringLiteral("\\s")));
    }

    // The stored lines that spell `full` in reading order once line breaks are ignored; empty when none do.
    std::vector<ui::CaptureTextItem> linesSpelling(const std::vector<ui::CaptureTextItem> &texts, const QString &full) {
        const QString target = squashed(full);
        for (std::size_t start = 0; start < texts.size(); ++start) {
            QString remaining = target;
            std::vector<ui::CaptureTextItem> lines;
            for (std::size_t i = start; i < texts.size() && !remaining.isEmpty(); ++i) {
                const QString line = squashed(texts[i].text);
                if (line.isEmpty() || !remaining.startsWith(line)) break;
                lines.push_back(texts[i]);
                remaining = remaining.sliced(line.size());
            }
            if (remaining.isEmpty() && !lines.empty()) return lines;
        }
        return {};
    }

    struct LiveText {
        QString text;
        QString objectName;
    };

    void collectItems(QQuickItem *item, std::vector<QQuickItem *> &out) {
        out.push_back(item);
        for (QQuickItem *child: item->childItems()) collectItems(child, out);
    }

    QQuickItem *itemNamed(const std::vector<QQuickItem *> &items, const QString &name) {
        const auto found = std::ranges::find_if(items, [&](const QQuickItem *item) {
            return item->objectName() == name;
        });
        return found == items.end() ? nullptr : *found;
    }

    QColor tintAt(const QString &color, const qreal alpha) {
        QColor tint(color);
        tint.setAlphaF(alpha);
        return tint;
    }

    class CaptureTableParity : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;
        std::unique_ptr<QQuickWindow> window;
        std::unique_ptr<QObject> outer;
        std::vector<QQuickItem *> liveItems;
        ui::CaptureAppearance appearance;
        QString wrappedNote;

        void SetUp() override {
            integration::clearSavedGraphConfiguration();
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
            const std::optional<double> best = fixturePersonalBest(env);
            ASSERT_TRUE(best.has_value() && *best > 0.0);
            ASSERT_TRUE(writeBenchmarkFile(env, benchmarkJson(*best)));

            app = startApp(env);
            ASSERT_NE(app, nullptr);
            auto *vm = app->benchmarkTrackingVm();
            vm->selectBenchmark(kBenchmarkId);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            ASSERT_EQ(vm->state(), QStringLiteral("incomplete"));
            ASSERT_EQ(vm->progressGrid()->content().rows.size(), static_cast<std::size_t>(kRowCount));
            ASSERT_EQ(vm->progressGrid()->content().tiers.size(), static_cast<std::size_t>(kTierCount));
            ASSERT_TRUE(vm->progressGrid()->content().hasSubcategories);

            appearance = ui::tableFixtureAppearance();
            ASSERT_NO_FATAL_FAILURE(createLiveGrid());
        }

        void TearDown() override {
            outer.reset();
            window.reset();
            app.reset();
            integration::clearSavedGraphConfiguration();
        }

        void createLiveGrid() {
            QQmlComponent component(app->engine());
            component.setData(R"(
                import QtQuick
                import KovaaksStatsViewer
                Flickable {
                    id: outer
                    property var gridModel
                    width: 600
                    height: 300
                    contentWidth: 600
                    contentHeight: grid.implicitHeight
                    clip: true
                    BenchmarkProgressGrid {
                        id: grid
                        width: 600
                        grid: outer.gridModel
                        benchmarkKey: "parity"
                    }
                }
            )", QUrl());
            ASSERT_FALSE(component.isError()) << component.errorString().toStdString();
            QObject *created = component.createWithInitialProperties(
                {{"gridModel", QVariant::fromValue(static_cast<QObject *>(app->benchmarkTrackingVm()->progressGrid()))}});
            ASSERT_NE(created, nullptr) << component.errorString().toStdString();
            outer.reset(created);

            window = std::make_unique<QQuickWindow>();
            window->resize(700, 400);
            auto *flickable = qobject_cast<QQuickItem *>(created);
            ASSERT_NE(flickable, nullptr);
            flickable->setParentItem(window->contentItem());
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            refreshLiveItems();

            QQuickItem *innerFlick = itemNamed(liveItems, QStringLiteral("progressGridFlick"));
            ASSERT_NE(innerFlick, nullptr);
            ASSERT_GT(innerFlick->property("contentWidth").toReal(), innerFlick->width())
                << "the grid must overflow horizontally";
            ASSERT_GT(created->property("contentHeight").toReal(), created->property("height").toReal())
                << "the grid must overflow vertically";
        }

        void refreshLiveItems() {
            liveItems.clear();
            collectItems(qobject_cast<QQuickItem *>(outer.get()), liveItems);
        }

        // Texts the live table shows right now, wherever they are scrolled to.
        std::vector<LiveText> visibleLiveTexts() {
            refreshLiveItems();
            std::vector<LiveText> texts;
            for (QQuickItem *item: liveItems) {
                if (!item->inherits("QQuickText") || !item->isVisible()) continue;
                const QString text = item->property("text").toString();
                if (!text.isEmpty()) texts.push_back({text, item->objectName()});
            }
            return texts;
        }

        std::optional<ui::TableCaptureSnapshot> snapshot() {
            return ui::captureTable(*app->benchmarkTrackingVm(), appearance);
        }

        // Records when a live text could only be found spread over several stored lines.
        bool contained(const std::vector<ui::CaptureTextItem> &texts, const QString &live) {
            if (std::ranges::any_of(texts, [&](const ui::CaptureTextItem &item) { return item.text == live; })) {
                return true;
            }
            if (linesSpelling(texts, live).empty()) return false;
            wrappedNote += live + QLatin1Char('|');
            return true;
        }

        void setScroll(const qreal fraction) {
            QQuickItem *innerFlick = itemNamed(liveItems, QStringLiteral("progressGridFlick"));
            ASSERT_NE(innerFlick, nullptr);
            auto *flickable = outer.get();
            innerFlick->setProperty("contentX",
                                    fraction * (innerFlick->property("contentWidth").toReal() - innerFlick->width()));
            flickable->setProperty("contentY",
                                   fraction * (flickable->property("contentHeight").toReal()
                                               - flickable->property("height").toReal()));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

            const QPointF offsets(innerFlick->property("contentX").toReal(), flickable->property("contentY").toReal());
            if (fraction > 0.0) {
                EXPECT_GT(offsets.x() + offsets.y(), 0.0) << "the live table did not scroll at " << fraction;
            }
            if (previousScroll) {
                EXPECT_NE(offsets, *previousScroll) << "the scroll did not move between iterations at " << fraction;
            }
            previousScroll = offsets;
        }

        std::optional<QPointF> previousScroll;
    };

    TEST_F(CaptureTableParity, EveryVisibleLiveTextIsInTheItemList) {
        const auto captured = snapshot();
        ASSERT_TRUE(captured.has_value());
        EXPECT_FALSE(captured->summary.has_value());
        const ui::CaptureLayout layout = ui::layoutTableCapture(*captured, false);
        const std::vector<ui::CaptureTextItem> texts = ui::captureTextItems(layout.items);

        const std::vector<LiveText> live = visibleLiveTexts();
        ASSERT_GT(live.size(), static_cast<std::size_t>(kRowCount));
        int thresholdLabels = 0;
        for (const LiveText &text: live) {
            EXPECT_TRUE(contained(texts, text.text)) << text.objectName.toStdString() << ": " << text.text.toStdString();
            if (text.objectName.contains(QStringLiteral("_label_"))) ++thresholdLabels;
        }
        EXPECT_GT(thresholdLabels, 0) << "the live bars showed no threshold label to compare";
        if (!wrappedNote.isEmpty()) RecordProperty("WrappedLiveTexts", wrappedNote.toStdString());
    }

    TEST_F(CaptureTableParity, ThresholdsHiddenLiveStayInTheLane) {
        const auto captured = snapshot();
        ASSERT_TRUE(captured.has_value());
        const ui::CaptureLayout layout = ui::layoutTableCapture(*captured, false);
        const std::vector<ui::CaptureTextItem> texts = ui::captureTextItems(layout.items);

        refreshLiveItems();
        int hiddenLive = 0;
        int played = 0;
        for (const presentation::ProgressGridScenarioRow &row: captured->grid.rows) {
            if (!row.hasPersonalBest || row.edgeState != presentation::ProgressGridEdgeState::None) continue;
            ++played;
            const QString bar = QStringLiteral("progressRow_%1_bar").arg(row.entryId);
            for (std::size_t tier = 0; tier < row.thresholdTexts.size(); ++tier) {
                const QString &threshold = row.thresholdTexts[tier];
                if (threshold.isEmpty() || row.missingChunks[tier]) continue;
                EXPECT_TRUE(contained(texts, threshold)) << row.name.toStdString() << " tier " << tier;

                const QQuickItem *label = itemNamed(liveItems, QStringLiteral("%1_label_%2").arg(bar).arg(tier));
                if (label && !label->isVisible()) ++hiddenLive;
            }
        }
        {
            QStringList dump;
            for (const auto &row: captured->grid.rows) {
                dump << QStringLiteral("%1 pb=%2 edge=%3 match=%4").arg(row.name, row.personalBestText)
                        .arg(static_cast<int>(row.edgeState)).arg(row.matchStateText);
            }
            EXPECT_GT(played, 0) << dump.join('\n').toStdString();
        }
        EXPECT_GT(hiddenLive, 0) << "no played chunk label was hidden live, so the lane was not shown to add one";
    }

    TEST_F(CaptureTableParity, ScrollPositionDoesNotReachTheCapture) {
        std::optional<ui::CaptureLayout> first;
        std::optional<ui::TableCaptureSnapshot> firstSnapshot;
        for (const qreal fraction: {0.0, 0.5, 1.0}) {
            ASSERT_NO_FATAL_FAILURE(setScroll(fraction));
            const auto captured = snapshot();
            ASSERT_TRUE(captured.has_value());
            const ui::CaptureLayout layout = ui::layoutTableCapture(*captured, false);
            if (!first) {
                first = layout;
                firstSnapshot = captured;
            }
            EXPECT_TRUE(ui::sameItems(first->items, layout.items)) << "at " << fraction;
            EXPECT_EQ(first->size, layout.size) << "at " << fraction;

            const auto &headings = captured->grid.headings;
            for (const QString &heading: {headings.category, headings.subcategory, headings.scenario}) {
                const auto texts = ui::captureTextItems(layout.items);
                EXPECT_EQ(std::ranges::count(texts, heading, &ui::CaptureTextItem::text), 1)
                    << heading.toStdString() << " at " << fraction;
            }
        }
    }

    TEST_F(CaptureTableParity, EveryRowAndGroupIsPresentInOrder) {
        const auto captured = snapshot();
        ASSERT_TRUE(captured.has_value());
        const presentation::ProgressGridContent &content = app->benchmarkTrackingVm()->progressGrid()->content();
        const ui::CaptureLayout layout = ui::layoutTableCapture(*captured, false);
        const std::vector<ui::CaptureTextItem> texts = ui::captureTextItems(layout.items);

        std::vector<QRectF> rowBands;
        for (const presentation::ProgressGridScenarioRow &row: content.rows) {
            const auto lines = linesSpelling(texts, row.name);
            ASSERT_FALSE(lines.empty()) << row.name.toStdString();
            const QRectF band = lines.front().rect.united(lines.back().rect);
            if (!rowBands.empty()) EXPECT_GT(band.top(), rowBands.back().top()) << row.name.toStdString();
            rowBands.push_back(band);
        }

        for (const presentation::ProgressGridGroup &group: content.groups) {
            ASSERT_GT(group.span, 0);
            const std::size_t last = static_cast<std::size_t>(group.start + group.span - 1);
            ASSERT_LT(last, rowBands.size());
            const auto nameLines = linesSpelling(texts, group.name);
            ASSERT_FALSE(nameLines.empty()) << group.name.toStdString();
            const qreal nameY = nameLines.front().rect.united(nameLines.back().rect).center().y();
            EXPECT_GE(nameY, rowBands[static_cast<std::size_t>(group.start)].top()) << group.name.toStdString();
            EXPECT_LE(nameY, rowBands[last].bottom()) << group.name.toStdString();
            if (group.color.isEmpty()) continue;

            const QColor tint = tintAt(group.color, captured->appearance.groupTintAlpha);
            std::optional<QRectF> cell;
            for (const ui::CaptureItem &item: layout.items) {
                const auto *rect = std::get_if<ui::CaptureRectItem>(&item);
                if (rect && rect->fill == tint && (!cell || rect->rect.height() > cell->height())) cell = rect->rect;
            }
            ASSERT_TRUE(cell.has_value()) << group.name.toStdString();

            for (std::size_t row = 0; row < rowBands.size(); ++row) {
                const bool inGroup = row >= static_cast<std::size_t>(group.start) && row <= last;
                const qreal y = rowBands[row].center().y();
                EXPECT_EQ(y >= cell->top() && y <= cell->bottom(), inGroup)
                    << group.name.toStdString() << " row " << row;
            }
        }
    }
}
