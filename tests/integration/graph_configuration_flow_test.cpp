//
// End-to-end checks of the graph-configuration feature over the REAL wired App: popup changes and
// settings visits as the user performs them, the configuration document as it is stored and
// upgraded, and the invariant that configuration is presentation only (AC08, AC09, AC11 / QUAL-03,
// STATE-08). Each test owns its App and clears the process-wide QSettings document it writes.
//

#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>
#include <QTest>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "app/app.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmark_history_vm.h"
#include "presentation/benchmark_tracking_vm.h"
#include "presentation/completion_history_vm.h"
#include "presentation/graph_vm.h"
#include "presentation/playtime_graph_vm.h"
#include "presentation/scenario_browser_vm.h"
#include "presentation/settings_vm.h"
#include "settings_service.h"

#include "integration_env.h"

using namespace ksv;
using namespace ksv::application;

namespace {
    constexpr long long kDayMs = 24LL * 60 * 60 * 1000;

    QJsonObject v3Series(const char *id, const char *name, const double position, const char *dsl,
                         const QJsonValue &yAxisId = QJsonValue()) {
        return QJsonObject{
            {"id", id},
            {
                "presentation", QJsonObject{
                    {"name", name}, {"enabled", true}, {"displayPosition", position},
                    {"lineStyle", QJsonObject{{"color", QJsonArray{1, 2, 3, 255}}, {"width", 2.0}}}
                }
            },
            {"expression", dsl}, {"yAxisId", yAxisId}, {"transformKind", "identity"}
        };
    }

    QJsonObject v3Axis(const char *id, const char *name) {
        return QJsonObject{
            {"id", id}, {"name", name},
            {
                "options", QJsonObject{
                    {"baseline", "hugData"}, {"integral", false}, {"targetTicks", 10}, {"fallbackSpan", 1.0}
                }
            },
            {"transformKind", "identity"}
        };
    }

    // A user computed series on a user axis with both allocators exhausted: what an upgrade must not lose.
    std::string v3Document() {
        const QJsonObject root{
            {"schemaVersion", 3}, {"nextComputedSeriesId", QJsonValue()},
            {
                "series", QJsonArray{
                    v3Series("1", "Score", 0, "SCORE"),
                    v3Series("12", "Custom", 1, "Add(SCORE, 2)", "7"),
                    v3Series("18446744073709551615", "Last", 2, "HITS")
                }
            },
            {"axes", QJsonArray{v3Axis("7", "Mine"), v3Axis("18446744073709551615", "Final")}},
            {"nextAxisId", QJsonValue()}
        };
        return QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
    }

    struct StyleProbe {
        int connection;
        bool markers;

        bool operator==(const StyleProbe &) const = default;
    };

    StyleProbe probe(const presentation::GraphViewModelBase *vm) { return {vm->connectionMode(), vm->pointMarkers()}; }

    constexpr auto kBenchmarkJson = R"({
        "schemaVersion": 1, "id": "auto-1", "name": "Auto",
        "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] } ],
        "uncategorized": [ { "id": "s1", "name": "1wall6targets TE", "hash": null,
            "thresholds": [ { "tierId": "t1", "score": 1000000000.0 } ] } ],
        "categories": []
    })";

    class GraphConfigurationFlowTest : public testing::Test {
    protected:
        integration::TestEnv env;
        std::unique_ptr<application::App> app;

        void SetUp() override {
            integration::clearSavedGraphConfiguration();
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
            writeBenchmark("auto.json", "auto-1");
        }

        void TearDown() override {
            app.reset();
            integration::clearSavedGraphConfiguration();
        }

        void writeBenchmark(const QString &file, const char *id) const {
            QFile out(QDir(env.benchmarksDir.path()).absoluteFilePath(file));
            ASSERT_TRUE(out.open(QIODevice::WriteOnly));
            QByteArray json = kBenchmarkJson;
            json.replace("auto-1", id);
            out.write(json);
        }

        // Stands in for a relaunch: a new SettingsService and persistence over the same storage, so
        // nothing but what was written can reach the new App.
        void startApp(const bool freshStorageHandles = false) {
            app.reset();
            auto settings = env.settings;
            auto persistence = env.graphConfigurationPersistence;
            if (freshStorageHandles) {
                settings = std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
                persistence = std::make_shared<qt_data::GraphConfigurationPersistence>(settings);
            }
            app = std::make_unique<application::App>(settings, std::make_shared<data::ProtoDecoder>(), persistence,
                                                     env.statsParser, env.benchmarkStore, env.daySource);
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->profileService()->isProfileLoaded(); }, 5000));
        }

        // Five runs of the fixture scenario on consecutive days and two of another scenario.
        void seedProfile() {
            auto base = app->profileService()->getLatestRun();
            ASSERT_EQ(base.run_id.scenario_id.name, "1wall6targets TE");
            domain::UserProfile profile;
            const auto source = profile.ensureSource(env.rootPath().toStdString(), "FPSAimTrainer/performances");
            base.sources.perf = {{source, "1wall6targets TE.perf"}};
            for (int i = 0; i < 5; ++i) {
                auto run = base;
                run.run_id.start_time += kDayMs * i;
                ASSERT_TRUE(profile.addRun(run));
            }
            for (int i = 0; i < 2; ++i) {
                auto run = base;
                run.run_id.scenario_id = {.name = "Other Scenario", .hash = "other-hash"};
                run.run_id.start_time += 10000 + kDayMs * i;
                ASSERT_TRUE(profile.addRun(run));
            }
            app->profileService()->applyBuiltProfile(std::move(profile));
            ASSERT_TRUE(QTest::qWaitFor([&] { return app->completionHistoryVm()->runCount() == 5; }, 5000));
        }

        void startSeededApp(const bool freshStorageHandles = false) {
            startApp(freshStorageHandles);
            ASSERT_NO_FATAL_FAILURE(seedProfile());
        }

        [[nodiscard]] presentation::SettingsViewModel *settings() const { return app->settingsVm(); }

        [[nodiscard]] GraphConfigurationSnapshot committed() const {
            return app->graphConfigurationUseCase()->current();
        }

        [[nodiscard]] bool quick(const char *graph, const QVariantMap &patch) const {
            return settings()->applyQuickPatch(graph, patch)["accepted"].toBool();
        }

        // Everything the dashboard shows of the graphs' configuration, as the user would see it.
        struct Dashboard {
            int visibleRuns;
            int playtimeWindow;
            StyleProbe run, playtime, history, rank, benchmarkPlaytime;

            bool operator==(const Dashboard &) const = default;
        };

        [[nodiscard]] Dashboard dashboard() const {
            return {
                app->completionHistoryVm()->visibleRunCount(),
                app->playtimeVm()->averagingWindowDays(),
                probe(app->graphVm()),
                probe(app->playtimeVm()),
                probe(app->completionHistoryVm()),
                probe(app->benchmarkTrackingVm()->rankHistory()),
                probe(app->benchmarkTrackingVm()->playtimeHistory()),
            };
        }
    };

    // ---- AC08 -------------------------------------------------------------------------------

    TEST_F(GraphConfigurationFlowTest, AQuickChangeSurvivesRestartAndDismissal) {
        ASSERT_NO_FATAL_FAILURE(startSeededApp());
        ASSERT_TRUE(quick("dailyPlaytime", {{"window", 9}, {"connection", "pointsOnly"}}));
        ASSERT_TRUE(quick("scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", 2}, {"connection", "straight"}}));
        EXPECT_EQ(app->completionHistoryVm()->visibleRunCount(), 2);
        const auto before = committed();
        EXPECT_FALSE(settings()->isVisitOpen()) << "quick changes need no settings visit";

        ASSERT_NO_FATAL_FAILURE(startSeededApp(true));

        EXPECT_EQ(*committed().configuration, *before.configuration);
        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), 9);
        EXPECT_EQ(app->playtimeVm()->title(), "Daily Playtime (9-day rolling average)");
        EXPECT_EQ(probe(app->playtimeVm()), (StyleProbe{2, false}));
        EXPECT_EQ(probe(app->completionHistoryVm()), (StyleProbe{1, true}));
        EXPECT_EQ(app->completionHistoryVm()->runCount(), 5);
        EXPECT_EQ(app->completionHistoryVm()->visibleRunCount(), 2);
        const auto quickSettings = settings()->quickSettings();
        EXPECT_EQ(quickSettings["dailyPlaytime"].toMap()["window"].toInt(), 9);
        EXPECT_EQ(quickSettings["scenarioHistory"].toMap()["rangeKind"].toString(), "latestRuns");
        EXPECT_EQ(quickSettings["scenarioHistory"].toMap()["rangeCount"].toInt(), 2);
        EXPECT_EQ(quickSettings["scenarioHistory"].toMap()["connection"].toString(), "straight");
    }

    TEST_F(GraphConfigurationFlowTest, AMultiGraphAndGraphLinesDraftReachesTheDashboardOnlyWhenSaved) {
        ASSERT_NO_FATAL_FAILURE(startSeededApp());
        const auto shownBefore = dashboard();
        const auto snapshotBefore = committed();
        const auto storedBefore = env.graphConfigurationPersistence->load();
        const auto enabledBefore = app->graphVm()->enabledSeriesIds();
        auto *vm = settings();

        vm->beginVisit();
        vm->setSelectedGraph("scenarioHistory");
        ASSERT_TRUE(vm->setGraphRangeKind("scenarioHistory", "latestRuns"));
        ASSERT_TRUE(vm->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", "2"));
        vm->setSelectedGraph("dailyPlaytime");
        ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "9"));
        vm->setSelectedGraph("benchmarkRank");
        ASSERT_TRUE(vm->setGraphConnection("benchmarkRank", "straight"));
        vm->setSelectedGraph("benchmarkPlaytime");
        ASSERT_TRUE(vm->setGraphField("benchmarkPlaytime", "dataRange", "", "averagingWindow", "12"));
        const auto created = vm->createComputedSeries("Draft line", QColor("#4CAF50"), 2.0, true, "");
        ASSERT_TRUE(created["succeeded"].toBool());
        const auto createdId = created["createdId"].toString();

        // Navigating between graphs keeps every staged edit.
        vm->setSelectedGraph("scenarioHistory");
        EXPECT_EQ(vm->graphDraft("scenarioHistory")["rangeKind"].toString(), "latestRuns");
        EXPECT_EQ(vm->graphDraft("scenarioHistory")["rangeCount"].toInt(), 2);
        EXPECT_EQ(vm->graphDraft("dailyPlaytime")["window"].toInt(), 9);
        EXPECT_EQ(vm->graphDraft("benchmarkRank")["connection"].toString(), "straight");
        EXPECT_EQ(vm->graphDraft("benchmarkPlaytime")["window"].toInt(), 12);
        EXPECT_TRUE(vm->hasPendingChanges());

        // Nothing is committed, drawn, or written.
        EXPECT_EQ(dashboard(), shownBefore);
        EXPECT_EQ(committed().revision, snapshotBefore.revision);
        EXPECT_EQ(*committed().configuration, *snapshotBefore.configuration);
        EXPECT_EQ(env.graphConfigurationPersistence->load(), storedBefore);
        EXPECT_EQ(app->graphVm()->enabledSeriesIds(), enabledBefore);
        EXPECT_EQ(vm->quickSettings()["dailyPlaytime"].toMap()["window"].toInt(), 3);

        const auto saved = vm->save();
        ASSERT_TRUE(saved["accepted"].toBool());

        EXPECT_FALSE(vm->hasPendingChanges());
        EXPECT_EQ(app->completionHistoryVm()->visibleRunCount(), 2);
        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), 9);
        EXPECT_EQ(probe(app->benchmarkTrackingVm()->rankHistory()), (StyleProbe{1, false}));
        EXPECT_TRUE(app->graphVm()->enabledSeriesIds().contains(createdId));
        EXPECT_EQ(env.graphConfigurationPersistence->load(), *committed().configuration);
        EXPECT_EQ(committed().configuration->preferences(GraphKey::BenchmarkPlaytime).averagingWindowDays, 12);
        vm->endVisit();
    }

    TEST_F(GraphConfigurationFlowTest, CancelAndDiscardBothDropTheDraftAndSaveMovesTheBaseline) {
        ASSERT_NO_FATAL_FAILURE(startSeededApp());
        const auto snapshotBefore = committed();
        const auto stored = env.graphConfigurationPersistence->load();
        auto *vm = settings();

        // Discard Changes: back to the seed, visit still open.
        vm->beginVisit();
        ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "9"));
        ASSERT_TRUE(vm->setGraphConnection("scenarioHistory", "pointsOnly"));
        ASSERT_TRUE(vm->createComputedSeries("Gone", QColor("#4CAF50"), 2.0, true, "")["succeeded"].toBool());
        vm->revertChanges();
        EXPECT_TRUE(vm->isVisitOpen());
        EXPECT_FALSE(vm->hasPendingChanges());
        EXPECT_EQ(vm->graphDraft("dailyPlaytime")["window"].toInt(), 3);
        EXPECT_EQ(vm->graphDraft("scenarioHistory")["connection"].toString(), "smooth");

        // Save moves the baseline: a later discard returns to what was saved, not to the seed.
        ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "9"));
        ASSERT_TRUE(vm->save()["accepted"].toBool());
        ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "20"));
        EXPECT_TRUE(vm->hasPendingChanges());
        vm->revertChanges();
        EXPECT_EQ(vm->graphDraft("dailyPlaytime")["window"].toInt(), 9);
        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), 9);

        // Cancel (the visit ends): nothing staged reaches the committed state or storage.
        ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "30"));
        ASSERT_TRUE(vm->setGraphConnection("benchmarkRank", "pointsOnly"));
        const auto revisionBeforeCancel = committed().revision;
        vm->endVisit();
        EXPECT_FALSE(vm->isVisitOpen());
        EXPECT_FALSE(vm->hasPendingChanges());
        EXPECT_EQ(committed().revision, revisionBeforeCancel);
        EXPECT_EQ(committed().configuration->preferences(GraphKey::DailyPlaytime).averagingWindowDays, 9);
        EXPECT_EQ(committed().configuration->preferences(GraphKey::BenchmarkRank).appearance.connection,
                  ConnectionMode::Smooth);
        EXPECT_EQ(env.graphConfigurationPersistence->load(), *committed().configuration);
        EXPECT_NE(snapshotBefore.revision, committed().revision);
        EXPECT_NE(stored, env.graphConfigurationPersistence->load());

        // The next visit seeds from what was committed, not from the dropped draft.
        vm->beginVisit();
        EXPECT_EQ(vm->graphDraft("dailyPlaytime")["window"].toInt(), 9);
        EXPECT_FALSE(vm->hasPendingChanges());
        vm->endVisit();
    }

    TEST_F(GraphConfigurationFlowTest, EarlierPopupChangesSurviveCancellingALaterVisit) {
        ASSERT_NO_FATAL_FAILURE(startSeededApp());
        ASSERT_TRUE(quick("dailyPlaytime", {{"window", 9}}));
        ASSERT_TRUE(quick("scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", 3}}));
        const auto popupState = committed();
        auto *vm = settings();

        vm->beginVisit();
        EXPECT_EQ(vm->graphDraft("dailyPlaytime")["window"].toInt(), 9) << "the visit seeds from the popup change";
        EXPECT_EQ(vm->graphDraft("scenarioHistory")["rangeCount"].toInt(), 3);
        ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "21"));
        ASSERT_TRUE(vm->setGraphConnection("scenarioHistory", "straight"));
        vm->endVisit();

        EXPECT_EQ(committed().revision, popupState.revision);
        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), 9);
        EXPECT_EQ(app->completionHistoryVm()->visibleRunCount(), 3);
        ASSERT_NO_FATAL_FAILURE(startSeededApp(true));
        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), 9);
        EXPECT_EQ(app->completionHistoryVm()->visibleRunCount(), 3);
    }

    TEST_F(GraphConfigurationFlowTest, AGraphsConfigurationFollowsItAcrossScenariosAndBenchmarks) {
        writeBenchmark("auto-2.json", "auto-2");
        ASSERT_NO_FATAL_FAILURE(startSeededApp());
        ASSERT_TRUE(quick("scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", 1}, {"connection", "straight"}}));
        ASSERT_TRUE(quick("benchmarkRank", {{"connection", "pointsOnly"}}));
        ASSERT_TRUE(quick("benchmarkPlaytime", {{"window", 7}}));
        const auto before = committed();
        auto *tracking = app->benchmarkTrackingVm();

        app->scenarioBrowserVm()->activateScenario("other-hash", "Other Scenario");
        ASSERT_TRUE(QTest::qWaitFor([&] { return app->completionHistoryVm()->scenarioTitle() == "Other Scenario"; }, 3000));
        EXPECT_EQ(app->completionHistoryVm()->runCount(), 2);
        EXPECT_EQ(app->completionHistoryVm()->visibleRunCount(), 1) << "the range applies to the other scenario too";
        EXPECT_EQ(probe(app->completionHistoryVm()).connection, 1);

        for (const char *benchmark: {"auto-1", "auto-2", "auto-1"}) {
            tracking->selectBenchmark(benchmark);
            ASSERT_TRUE(QTest::qWaitFor([&] { return tracking->state() == "trackable"; }, 3000))
                << tracking->state().toStdString();
            EXPECT_EQ(probe(tracking->rankHistory()).connection, 2) << benchmark;
        }
        tracking->clearSelection();

        EXPECT_EQ(committed().revision, before.revision) << "selection never edits the configuration";
        EXPECT_EQ(*committed().configuration, *before.configuration);
    }

    // ---- AC09 -------------------------------------------------------------------------------

    TEST_F(GraphConfigurationFlowTest, ResetStagesOneGraphsDefaultsAndKeepsDefinitionsAxesAndOtherEdits) {
        ASSERT_NO_FATAL_FAILURE(startSeededApp());
        auto *vm = settings();
        vm->beginVisit();
        const auto axis = vm->createAxis("My axis");
        ASSERT_TRUE(axis["succeeded"].toBool());
        const auto series = vm->createComputedSeries("Mine", QColor("#4CAF50"), 2.0, true, "");
        ASSERT_TRUE(series["succeeded"].toBool());
        ASSERT_TRUE(vm->updateSeriesAxis(series["createdId"].toString(), axis["createdId"].toString())["succeeded"].toBool());
        vm->setRunAxisLabelSeriesId(series["createdId"].toString());
        ASSERT_TRUE(vm->setGraphRangeKind("scenarioHistory", "latestRuns"));
        ASSERT_TRUE(vm->setGraphField("scenarioHistory", "dataRange", "", "latestRuns", "2"));
        ASSERT_TRUE(vm->setGraphConnection("scenarioHistory", "straight"));
        ASSERT_TRUE(vm->setGraphField("dailyPlaytime", "dataRange", "", "averagingWindow", "9"));
        ASSERT_TRUE(vm->setGraphConnection("run", "straight"));
        ASSERT_TRUE(vm->save()["accepted"].toBool());
        const auto saved = committed();
        const auto lines = vm->getAllSeriesConfigs();
        const auto axes = vm->getAllAxes();
        const auto label = vm->runAxisLabelSeriesId();
        ASSERT_FALSE(label.isEmpty());

        // Reset is a draft edit on the selected graph only.
        ASSERT_TRUE(vm->resetGraph("scenarioHistory"));
        EXPECT_TRUE(vm->hasPendingChanges());
        EXPECT_EQ(vm->graphDraft("scenarioHistory")["rangeKind"].toString(), "allHistory");
        EXPECT_EQ(vm->graphDraft("scenarioHistory")["connection"].toString(), "smooth");
        EXPECT_EQ(vm->graphDraft("dailyPlaytime")["window"].toInt(), 9);
        EXPECT_EQ(vm->graphDraft("run")["connection"].toString(), "straight");
        EXPECT_EQ(vm->getAllSeriesConfigs(), lines);
        EXPECT_EQ(vm->getAllAxes(), axes);
        EXPECT_EQ(vm->runAxisLabelSeriesId(), label);
        EXPECT_EQ(committed().revision, saved.revision) << "nothing is applied before Save";
        EXPECT_EQ(app->completionHistoryVm()->visibleRunCount(), 2);

        // Cancellable.
        vm->revertChanges();
        EXPECT_EQ(vm->graphDraft("scenarioHistory")["rangeKind"].toString(), "latestRuns");
        EXPECT_FALSE(vm->hasPendingChanges());

        // Saved: that graph returns to defaults; definitions, axes and every other graph are intact.
        ASSERT_TRUE(vm->resetGraph("scenarioHistory"));
        ASSERT_TRUE(vm->resetGraph("run"));
        ASSERT_TRUE(vm->save()["accepted"].toBool());
        const auto after = *committed().configuration;
        EXPECT_EQ(after.preferences(GraphKey::ScenarioHistory), defaultGraphPreferences(GraphKey::ScenarioHistory));
        EXPECT_EQ(after.preferences(GraphKey::Run), defaultGraphPreferences(GraphKey::Run));
        EXPECT_EQ(after.preferences(GraphKey::DailyPlaytime), saved.configuration->preferences(GraphKey::DailyPlaytime));
        EXPECT_EQ(after.runSeries, saved.configuration->runSeries);
        EXPECT_EQ(after.runAxisLabelSeriesId, saved.configuration->runAxisLabelSeriesId);
        EXPECT_EQ(vm->getAllSeriesConfigs(), lines);
        EXPECT_EQ(vm->getAllAxes(), axes);
        EXPECT_EQ(app->completionHistoryVm()->visibleRunCount(), 5);
        EXPECT_EQ(env.graphConfigurationPersistence->load(), after);
        vm->endVisit();
    }

    TEST_F(GraphConfigurationFlowTest, AVersionThreeDocumentUpgradesWithEveryDefaultIntact) {
        env.settings->setSeriesConfigDocument(v3Document());
        QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        raw.setValue("graphAxis/seriesId", "12");
        raw.sync();

        ASSERT_NO_FATAL_FAILURE(startSeededApp());

        const auto loaded = *committed().configuration;
        for (const auto key: kGraphKeys)
            EXPECT_EQ(loaded.preferences(key), defaultGraphPreferences(key)) << graphKeyName(key);
        ASSERT_EQ(loaded.runSeries.series.size(), 3U);
        EXPECT_EQ(loaded.runSeries.series[1].id, SeriesId{12});
        EXPECT_EQ(loaded.runSeries.series[1].yAxisId, AxisId{7});
        ASSERT_EQ(loaded.runSeries.axes.size(), 2U);
        EXPECT_FALSE(loaded.runSeries.nextSeriesId.has_value());
        EXPECT_FALSE(loaded.runSeries.nextAxisId.has_value());
        EXPECT_EQ(loaded.runAxisLabelSeriesId, SeriesId{12});
        EXPECT_EQ(app->graphVm()->axisLabelSeriesId(), "12");

        // The same look as before graph configuration existed.
        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), 3);
        EXPECT_EQ(dashboard().visibleRuns, 5);
        EXPECT_EQ(probe(app->graphVm()), (StyleProbe{0, false}));
        EXPECT_EQ(probe(app->completionHistoryVm()), (StyleProbe{0, true}));

        // Loading leaves a valid v3 document as it is; the first accepted write stores version 4 with the
        // old sections untouched, and nothing is quarantined.
        QSettings stored(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        const auto storedDocument = [&] {
            stored.sync();
            return QJsonDocument::fromJson(stored.value("graph/seriesConfigV1").toString().toUtf8()).object();
        };
        const auto original = QJsonDocument::fromJson(QByteArray::fromStdString(v3Document())).object();
        EXPECT_EQ(storedDocument()["schemaVersion"].toInt(), 3);
        ASSERT_TRUE(quick("dailyPlaytime", {{"window", 4}}));
        ASSERT_TRUE(quick("dailyPlaytime", {{"window", 3}}));
        const auto upgraded = storedDocument();
        EXPECT_EQ(upgraded["schemaVersion"].toInt(), 4);
        EXPECT_EQ(upgraded["series"], original["series"]);
        EXPECT_EQ(upgraded["axes"], original["axes"]);
        stored.beginGroup("graph/seriesConfigQuarantine");
        EXPECT_TRUE(stored.childKeys().isEmpty());
        stored.endGroup();

        // Later loads prefer the document over the legacy selector.
        raw.setValue("graphAxis/seriesId", "1");
        raw.sync();
        ASSERT_NO_FATAL_FAILURE(startSeededApp(true));
        EXPECT_EQ(committed().configuration->runAxisLabelSeriesId, SeriesId{12});
        EXPECT_EQ(*committed().configuration, loaded);
    }

    // ---- QUAL-03 ----------------------------------------------------------------------------

    struct SourceFingerprint {
        QStringList lines;

        bool operator==(const SourceFingerprint &) const = default;
    };

    SourceFingerprint fingerprint(application::App &app, const integration::TestEnv &env) {
        QStringList lines;
        auto scenarios = app.profileService()->getScenarioList();
        std::ranges::sort(scenarios, [](const domain::ScenarioId &a, const domain::ScenarioId &b) {
            return a.hash < b.hash;
        });
        for (const auto &scenario: scenarios) {
            lines << QString("%1|%2|runs=%3|time=%4").arg(scenario.name.c_str(), scenario.hash.c_str())
                    .arg(app.profileService()->getRunCount(scenario).value_or(0))
                    .arg(app.profileService()->getTotalTime(scenario).value_or(-1.0));
            auto runs = app.profileService()->getRunsForScenario(scenario);
            std::ranges::sort(runs, [](const domain::Run &a, const domain::Run &b) {
                return a.run_id.start_time < b.run_id.start_time;
            });
            for (const auto &run: runs) {
                lines << QString("%1|%2|%3|%4|%5|%6|%7").arg(run.run_id.start_time)
                        .arg(run.totals().score).arg(run.totals().shots).arg(run.totals().hits)
                        .arg(run.totals().misses).arg(run.totals().kills)
                        .arg(run.sources.perf ? run.sources.perf->filename.c_str() : "-");
            }
        }
        const QDir benchmarks(env.benchmarksDir.path());
        for (const auto &file: benchmarks.entryList({"*.json"}, QDir::Files, QDir::Name)) {
            QFile in(benchmarks.absoluteFilePath(file));
            if (!in.open(QIODevice::ReadOnly)) lines << file + "|unreadable";
            lines << file + "|" + QCryptographicHash::hash(in.readAll(), QCryptographicHash::Sha1).toHex();
        }
        if (const auto *projection = app.benchmarkTrackingUseCase()->projection()) {
            lines << QString("benchmark|play=%1|avg=%2|history=%3").arg(projection->totalPlaytimeSeconds)
                    .arg(projection->averageRank.value_or(-1.0)).arg(projection->averageRankHistory.size());
            for (const auto &scenario: projection->scenarios)
                lines << QString("entry|%1|runs=%2|pb=%3|play=%4").arg(scenario.name.c_str()).arg(scenario.runCount)
                        .arg(scenario.personalBest.value_or(-1.0)).arg(scenario.playtimeSeconds);
        }
        return {lines};
    }

    TEST_F(GraphConfigurationFlowTest, NoConfigurationChangeTouchesSourceRecordsDefinitionsOrTotals) {
        ASSERT_NO_FATAL_FAILURE(startSeededApp());
        app->benchmarkTrackingVm()->selectBenchmark("auto-1");
        ASSERT_TRUE(QTest::qWaitFor([&] { return app->benchmarkTrackingVm()->state() == "trackable"; }, 3000));
        const auto before = fingerprint(*app, env);
        ASSERT_GT(before.lines.size(), 8);
        const auto runSeries = committed().configuration->runSeries;
        const auto selectedRun = app->sessionController()->getCurrentRun().run_id;
        const auto scenarios = app->profileService()->getScenarioList();
        auto *vm = settings();

        // Popup changes of every kind, on every graph.
        for (const char *graph: {"dailyPlaytime", "benchmarkPlaytime"}) {
            EXPECT_TRUE(quick(graph, {{"rangeKind", "recentDays"}, {"count", 1}, {"window", 1}}));
            EXPECT_TRUE(quick(graph, {{"rangeKind", "allHistory"}, {"window", 3650}}));
        }
        EXPECT_TRUE(quick("scenarioHistory", {{"rangeKind", "latestRuns"}, {"count", 1}}));
        EXPECT_TRUE(quick("benchmarkRank", {{"rangeKind", "recentDays"}, {"count", 1}}));
        for (const auto key: kGraphKeys) {
            EXPECT_TRUE(quick(std::string(graphKeyName(key)).c_str(), {{"connection", "pointsOnly"}, {"markers", true}}));
        }

        // A visit with fixed axes, widths, colours, resets and a saved Save.
        vm->beginVisit();
        EXPECT_TRUE(vm->setAxisMode("scenarioHistory", "score", "fixed"));
        EXPECT_TRUE(vm->setGraphField("scenarioHistory", "axes", "score", "fixedMin", "0"));
        EXPECT_TRUE(vm->setGraphField("scenarioHistory", "axes", "score", "fixedMax", "10"));
        EXPECT_TRUE(vm->setGraphField("dailyPlaytime", "seriesStyle", "playtime", "width", "9"));
        EXPECT_TRUE(vm->setSeriesColor("benchmarkRank", "rank", QColor("#123456")));
        EXPECT_TRUE(vm->setGraphAnchor("dailyPlaytime", true));
        ASSERT_TRUE(vm->save()["accepted"].toBool());
        EXPECT_TRUE(vm->resetGraph("benchmarkPlaytime"));
        EXPECT_TRUE(vm->resetGraph("run"));
        ASSERT_TRUE(vm->save()["accepted"].toBool());
        vm->endVisit();

        // Calendar rollover under today-anchored ranges.
        env.daySource->advance(3);
        EXPECT_TRUE(quick("dailyPlaytime", {{"rangeKind", "recentDays"}, {"count", 7}}));
        env.daySource->advance(40);

        EXPECT_EQ(fingerprint(*app, env), before);
        EXPECT_EQ(committed().configuration->runSeries, runSeries);
        EXPECT_EQ(app->sessionController()->getCurrentRun().run_id, selectedRun);
        EXPECT_EQ(app->profileService()->getScenarioList(), scenarios);
        EXPECT_EQ(app->completionHistoryVm()->runCount(), 5) << "the full history keeps its size however it is windowed";
    }
}
