//
// Exercises the REAL composition root (App::App) end to end: the wired graph
// ProfileService -> SessionController and the onProfileChanged -> PlaytimeVm
// refresh cascade that only ever gets assembled at runtime. Uses App's own
// accessors so the test can never drift from the actual wiring.
//

#include <gtest/gtest.h>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <memory>
#include <type_traits>
#include <chrono>
#include <vector>

#include "app/app.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmark_tracking_vm.h"
#include "presentation/completion_history_vm.h"
#include "presentation/playtime_graph_vm.h"
#include "presentation/settings_vm.h"

#include "integration_env.h"

using namespace ksv;

namespace {
    // One App for the whole suite. Building it runs loadProfile(), which asks
    // SessionController for a build on its worker thread when no store is available,
    // and the result only lands once the event loop spins -- a wait every test used to
    // repeat for a structurally identical graph. The tests that mutate the wired state
    // (current run, profile) each re-establish their own starting point rather than
    // relying on a fresh graph.
    class CompositionRootTest : public testing::Test {
    protected:
        static std::unique_ptr<integration::TestEnv> env;
        static std::unique_ptr<application::App> app;

        static void SetUpTestSuite() {
            env = std::make_unique<integration::TestEnv>();
            ASSERT_TRUE(env->valid());
            ASSERT_TRUE(env->makePerformancesDir());
            ASSERT_FALSE(env->copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
            ASSERT_FALSE(env->copyFixtureIntoPerformances("VT FlyTS Novice S5.perf").isEmpty());

            app = std::make_unique<application::App>(
                env->settings, std::make_shared<data::ProtoDecoder>(), env->graphConfigurationPersistence,
                env->statsParser, env->benchmarkStore, env->daySource);
            ASSERT_TRUE(waitForProfile());
        }

        // App holds shared_ptrs to services built over env's settings, so it goes first.
        static void TearDownTestSuite() {
            app.reset();
            env.reset();
        }

        [[nodiscard]] static bool waitForProfile() {
            QElapsedTimer timer;
            timer.start();
            while (!app->profileService()->isProfileLoaded()) {
                if (timer.elapsed() > 5000) return false;
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            }
            return true;
        }

        [[nodiscard]] static QString fixture(const QString &name) {
            return QDir(env->performancesDir()).absoluteFilePath(name);
        }
    };

    std::unique_ptr<integration::TestEnv> CompositionRootTest::env;
    std::unique_ptr<application::App> CompositionRootTest::app;

    TEST_F(CompositionRootTest, ConstructionBuildsProfileFromTheKovaaksDirectory) {
        EXPECT_TRUE(app->profileService()->isProfileLoaded());
        EXPECT_FALSE(app->profileService()->getScenarioList().empty());
    }

    TEST_F(CompositionRootTest, SessionControllerReadsThroughToTheRealProfile) {
        // SessionController::getScenarioList delegates to the same wired ProfileService.
        EXPECT_EQ(app->sessionController()->getScenarioList().size(),
                  app->profileService()->getScenarioList().size());
    }

    TEST_F(CompositionRootTest, SetCurrentPerfResolvesThroughFileServiceAndDecoder) {
        app->sessionController()->setCurrentPerf(fixture("1wall6targets TE.perf").toStdString());

        EXPECT_EQ(app->sessionController()->getCurrentRun().run_id.scenario_id.name, "1wall6targets TE");
    }

    TEST_F(CompositionRootTest, CurrentPerfChangeCascadesToGraphViewModelReload) {
        // App wires sessionController->currentRunChanged -> graphVm->fetchData() so the
        // graph reloads whenever currentPerf changes for any reason, not just file loads.
        // setCurrentRun ignores a run_id it is already on, and this suite shares one App,
        // so park on the other fixture first to guarantee the call below is a real change.
        app->sessionController()->setCurrentPerf(fixture("VT FlyTS Novice S5.perf").toStdString());

        QSignalSpy spy(app->graphVm(), &presentation::GraphViewModelBase::dataUpdated);
        ASSERT_TRUE(spy.isValid());

        app->sessionController()->setCurrentPerf(fixture("1wall6targets TE.perf").toStdString());

        EXPECT_GE(spy.count(), 1);
    }

    TEST_F(CompositionRootTest, ProfileChangeCascadesToPlaytimeViewModelRefresh) {
        // PlaytimeGraphUseCase subscribes to the profile and the playtime VM to the use case;
        // refresh() emits dataUpdated, so a profile mutation must reach the playtime VM.
        QSignalSpy spy(app->playtimeVm(), &presentation::GraphViewModelBase::dataUpdated);
        ASSERT_TRUE(spy.isValid());

        app->profileService()->generateProfileFromDirectory();

        EXPECT_GE(spy.count(), 1);
    }

    TEST_F(CompositionRootTest, ACommittedDailyPlaytimeWindowRelabelsTheDashboardGraph) {
        application::GraphQuickPatch patch;
        patch.averagingWindowDays = 9;
        ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(application::GraphKey::DailyPlaytime, patch).accepted());

        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), 9);
        EXPECT_EQ(app->playtimeVm()->title(), "Daily Playtime (9-day rolling average)");

        patch.averagingWindowDays = application::kDefaultAveragingWindowDays;
        ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(application::GraphKey::DailyPlaytime, patch).accepted());
        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), application::kDefaultAveragingWindowDays);
    }

    TEST_F(CompositionRootTest, ABenchmarkPlaytimeWindowChangeLeavesTheOtherGraphsWindowAlone) {
        application::GraphQuickPatch patch;
        patch.averagingWindowDays = 12;
        ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(application::GraphKey::BenchmarkPlaytime, patch).accepted());

        EXPECT_EQ(app->playtimeVm()->averagingWindowDays(), application::kDefaultAveragingWindowDays);
        const auto committed = app->graphConfigurationUseCase()->current();
        EXPECT_EQ(committed.configuration->preferences(application::GraphKey::BenchmarkPlaytime).averagingWindowDays, 12);
        EXPECT_EQ(committed.configuration->preferences(application::GraphKey::DailyPlaytime).averagingWindowDays,
                  application::kDefaultAveragingWindowDays);
    }

    TEST_F(CompositionRootTest, TheInjectedDaySourceIsTheOneTheAppExposesAndAdvances) {
        ASSERT_EQ(app->daySource(), env->daySource);

        std::vector<std::chrono::sys_days> announced;
        app->daySource()->onDayChanged([&](const std::chrono::sys_days day) { announced.push_back(day); });
        const auto before = app->daySource()->today();
        env->daySource->advance();

        EXPECT_EQ(app->daySource()->today(), before + std::chrono::days{1});
        EXPECT_EQ(announced, std::vector{before + std::chrono::days{1}});
    }

    TEST_F(CompositionRootTest, TheUseCaseReadsTheServicesSingleCommittedSnapshot) {
        const auto viaUseCase = app->graphConfigurationUseCase()->current();
        const auto viaService = app->graphConfigurationService()->snapshot();

        EXPECT_EQ(viaUseCase.configuration, viaService.configuration);
        EXPECT_EQ(viaUseCase.revision, viaService.revision);
    }

    // The run graph draws committed configuration only: a settings visit's Graph Lines and axis-label
    // edits stay off the dashboard until the visit saves, and the save goes through the injected
    // persistence port.
    TEST_F(CompositionRootTest, SettingsVisitEditsReachTheRunGraphOnlyWhenSaved) {
        auto *settingsVm = app->settingsVm();
        settingsVm->beginVisit();
        const auto before = app->graphVm()->enabledSeriesIds();
        const auto labelBefore = app->graphVm()->axisLabelSeriesId();
        const auto writtenBefore = env->graphConfigurationPersistence->load();

        const auto created = settingsVm->createComputedSeries("Draft line", QColor("#4CAF50"), 2.0, true, "");
        ASSERT_TRUE(created["succeeded"].toBool());
        const auto id = created["createdId"].toString();
        settingsVm->setRunAxisLabelSeriesId(id);

        EXPECT_TRUE(settingsVm->hasPendingChanges());
        EXPECT_EQ(app->graphVm()->enabledSeriesIds(), before);
        EXPECT_EQ(app->graphVm()->axisLabelSeriesId(), labelBefore);
        EXPECT_EQ(env->graphConfigurationPersistence->load(), writtenBefore);

        ASSERT_TRUE(settingsVm->save()["accepted"].toBool());

        EXPECT_FALSE(settingsVm->hasPendingChanges());
        EXPECT_TRUE(settingsVm->isVisitOpen());
        EXPECT_TRUE(app->graphVm()->enabledSeriesIds().contains(id));
        EXPECT_EQ(app->graphVm()->axisLabelSeriesId(), id);
        const auto stored = env->graphConfigurationPersistence->load();
        EXPECT_EQ(stored, *app->graphConfigurationService()->snapshot().configuration);
        settingsVm->endVisit();
    }

    TEST_F(CompositionRootTest, EndingASettingsVisitDropsItsEditsWithoutWriting) {
        auto *settingsVm = app->settingsVm();
        settingsVm->beginVisit();
        const auto before = app->graphVm()->enabledSeriesIds();
        const auto committed = app->graphConfigurationService()->snapshot();
        ASSERT_TRUE(settingsVm->createComputedSeries("Dropped", QColor("#4CAF50"), 2.0, true, "")["succeeded"].toBool());

        settingsVm->endVisit();

        EXPECT_FALSE(settingsVm->hasPendingChanges());
        EXPECT_EQ(app->graphVm()->enabledSeriesIds(), before);
        EXPECT_EQ(app->graphConfigurationService()->snapshot().revision, committed.revision);
        EXPECT_EQ(env->graphConfigurationPersistence->load(), *committed.configuration);
    }

    // ---- committed graph style reaches every graph view model --------------------------------

    struct StyleProbe {
        int connection;
        bool markers;

        bool operator==(const StyleProbe &) const = default;
    };

    StyleProbe probe(const presentation::GraphViewModelBase *vm) { return {vm->connectionMode(), vm->pointMarkers()}; }

    StyleProbe defaultProbe(const application::GraphKey key) {
        const auto appearance = application::defaultGraphPreferences(key).appearance;
        return {static_cast<int>(appearance.connection), appearance.lineModeMarkers};
    }

    QColor colorOf(const application::SeriesStyle &style) {
        const auto color = style.color.value();
        return {color.red, color.green, color.blue, color.alpha};
    }

    // STATE-08: nothing saved means every graph draws as it did before graph configuration existed.
    TEST_F(CompositionRootTest, WithNoSavedStyleEveryGraphLooksAsItAlwaysDid) {
        using application::GraphKey;
        EXPECT_EQ(probe(app->graphVm()), defaultProbe(GraphKey::Run));
        EXPECT_EQ(probe(app->playtimeVm()), defaultProbe(GraphKey::DailyPlaytime));
        EXPECT_EQ(probe(app->completionHistoryVm()), defaultProbe(GraphKey::ScenarioHistory));
        EXPECT_EQ(probe(app->benchmarkTrackingVm()->rankHistory()), defaultProbe(GraphKey::BenchmarkRank));
        EXPECT_EQ(probe(app->benchmarkTrackingVm()->playtimeHistory()), defaultProbe(GraphKey::BenchmarkPlaytime));
        const auto score = app->completionHistoryVm()->series({presentation::CompletionHistoryViewModel::Score});
        const auto defaultScore = application::defaultSeriesStyle(GraphKey::ScenarioHistory, "score");
        EXPECT_EQ(score.front()->color(), colorOf(defaultScore));
        EXPECT_DOUBLE_EQ(score.front()->lineWidth, defaultScore.width);
    }

    // SCOPE-03: a graph type owns its style; committing one never moves another.
    TEST_F(CompositionRootTest, AQuickStyleChangeReachesOnlyItsOwnGraph) {
        using application::GraphKey;
        const auto *configuration = app->graphConfigurationUseCase().get();
        application::GraphQuickPatch patch;
        patch.connection = application::ConnectionMode::PointsOnly;
        patch.seriesStyles["playtime"] = {application::RgbaColor{1, 2, 3, 255}, 7.0};
        ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(GraphKey::DailyPlaytime, patch).accepted());

        EXPECT_EQ(probe(app->playtimeVm()),
                  (StyleProbe{static_cast<int>(application::ConnectionMode::PointsOnly),
                              defaultProbe(GraphKey::DailyPlaytime).markers}));
        const auto *daily = app->playtimeVm()->series({presentation::PlaytimeGraphViewModel::Playtime}).front();
        EXPECT_EQ(daily->color(), QColor(1, 2, 3));
        EXPECT_DOUBLE_EQ(daily->lineWidth, 7.0);
        EXPECT_EQ(probe(app->graphVm()), defaultProbe(GraphKey::Run));
        EXPECT_EQ(probe(app->completionHistoryVm()), defaultProbe(GraphKey::ScenarioHistory));
        EXPECT_EQ(probe(app->benchmarkTrackingVm()->rankHistory()), defaultProbe(GraphKey::BenchmarkRank));
        EXPECT_EQ(probe(app->benchmarkTrackingVm()->playtimeHistory()), defaultProbe(GraphKey::BenchmarkPlaytime));
        EXPECT_EQ(configuration->current().configuration->preferences(GraphKey::ScenarioHistory),
                  application::defaultGraphPreferences(GraphKey::ScenarioHistory));

        application::GraphQuickPatch restore;
        restore.connection = application::ConnectionMode::Smooth;
        restore.seriesStyles["playtime"] = {};
        ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(GraphKey::DailyPlaytime, restore).accepted());
        EXPECT_EQ(probe(app->playtimeVm()), defaultProbe(GraphKey::DailyPlaytime));
        EXPECT_EQ(app->playtimeVm()->series({presentation::PlaytimeGraphViewModel::Playtime}).front()->color(),
                  colorOf(application::defaultSeriesStyle(GraphKey::DailyPlaytime, "playtime")));
    }

    TEST_F(CompositionRootTest, EachGraphKindTakesItsStyleThroughItsOwnWiring) {
        using application::GraphKey;
        const auto commit = [&](const GraphKey key, const application::ConnectionMode mode, const bool markers) {
            application::GraphQuickPatch patch;
            patch.connection = mode;
            patch.lineModeMarkers = markers;
            ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(key, patch).accepted());
        };

        commit(GraphKey::Run, application::ConnectionMode::Straight, true);
        commit(GraphKey::ScenarioHistory, application::ConnectionMode::PointsOnly, false);
        commit(GraphKey::BenchmarkRank, application::ConnectionMode::Straight, false);
        commit(GraphKey::BenchmarkPlaytime, application::ConnectionMode::PointsOnly, true);

        EXPECT_EQ(probe(app->graphVm()), (StyleProbe{1, true}));
        EXPECT_EQ(probe(app->completionHistoryVm()), (StyleProbe{2, false}));
        EXPECT_EQ(probe(app->benchmarkTrackingVm()->rankHistory()), (StyleProbe{1, false}));
        EXPECT_EQ(probe(app->benchmarkTrackingVm()->playtimeHistory()), (StyleProbe{2, true}));
        EXPECT_EQ(probe(app->playtimeVm()), (StyleProbe{0, false}));

        commit(GraphKey::Run, application::ConnectionMode::Smooth, false);
        commit(GraphKey::ScenarioHistory, application::ConnectionMode::Smooth, true);
        commit(GraphKey::BenchmarkRank, application::ConnectionMode::Smooth, false);
        commit(GraphKey::BenchmarkPlaytime, application::ConnectionMode::Smooth, false);
        EXPECT_EQ(probe(app->completionHistoryVm()), (StyleProbe{0, true}));
        EXPECT_EQ(probe(app->benchmarkTrackingVm()->playtimeHistory()), (StyleProbe{0, false}));
    }

    // QUAL-04: a style change redraws; it does not re-read or recompute any history.
    TEST_F(CompositionRootTest, AStyleChangeRestylesWithoutRefreshingAnyGraphData) {
        using application::GraphKey;
        QSignalSpy playtimeData(app->playtimeVm(), &presentation::GraphViewModelBase::dataUpdated);
        QSignalSpy historyData(app->completionHistoryVm(), &presentation::GraphViewModelBase::dataUpdated);
        QSignalSpy runData(app->graphVm(), &presentation::GraphViewModelBase::dataUpdated);
        QSignalSpy rankData(app->benchmarkTrackingVm()->rankHistory(), &presentation::GraphViewModelBase::dataUpdated);
        QSignalSpy playtimeStyle(app->playtimeVm(), &presentation::GraphViewModelBase::styleChanged);

        application::GraphQuickPatch patch;
        patch.connection = application::ConnectionMode::Straight;
        for (const auto key: {GraphKey::Run, GraphKey::DailyPlaytime, GraphKey::ScenarioHistory,
                              GraphKey::BenchmarkRank, GraphKey::BenchmarkPlaytime})
            ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(key, patch).accepted());

        EXPECT_EQ(playtimeData.count(), 0);
        EXPECT_EQ(historyData.count(), 0);
        EXPECT_EQ(runData.count(), 0);
        EXPECT_EQ(rankData.count(), 0);
        EXPECT_EQ(playtimeStyle.count(), 1);

        patch.connection = application::ConnectionMode::Smooth;
        for (const auto key: {GraphKey::Run, GraphKey::DailyPlaytime, GraphKey::ScenarioHistory,
                              GraphKey::BenchmarkRank, GraphKey::BenchmarkPlaytime})
            ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(key, patch).accepted());
    }

    // The rank line keeps its identifying name however it is recoloured, and the selected benchmark
    // never decides the style.
    TEST_F(CompositionRootTest, ARecolouredRankLineKeepsItsNameAcrossBenchmarkSelectionChanges) {
        using application::GraphKey;
        application::GraphQuickPatch patch;
        patch.seriesStyles["rank"] = {application::RgbaColor{9, 99, 199, 255}, 5.0};
        ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(GraphKey::BenchmarkRank, patch).accepted());

        const auto *rank = app->benchmarkTrackingVm()->rankHistory();
        EXPECT_EQ(rank->metricName(), "Personal-best average rank");
        app->benchmarkTrackingVm()->clearSelection();
        app->benchmarkTrackingVm()->selectDefaultBenchmark();
        EXPECT_EQ(rank->metricName(), "Personal-best average rank");
        EXPECT_EQ(app->benchmarkTrackingUseCase()->presentation(GraphKey::BenchmarkRank).seriesStyle("rank").color,
                  (application::RgbaColor{9, 99, 199, 255}));

        patch.seriesStyles["rank"] = {};
        ASSERT_TRUE(app->graphConfigurationUseCase()->applyQuickPatch(GraphKey::BenchmarkRank, patch).accepted());
    }

    // A saved style is in force from the first frame, not only after the next change.
    TEST(CompositionRootStartup, SavedStyleIsAppliedByEveryGraphViewModelAtConstruction) {
        integration::TestEnv startupEnv;
        ASSERT_TRUE(startupEnv.valid());
        ASSERT_TRUE(startupEnv.makePerformancesDir());
        auto saved = application::defaultGraphConfiguration();
        saved.preferences(application::GraphKey::Run).appearance = {application::ConnectionMode::Straight, true};
        saved.preferences(application::GraphKey::DailyPlaytime).appearance.connection =
            application::ConnectionMode::PointsOnly;
        saved.preferences(application::GraphKey::ScenarioHistory).appearance = {
            application::ConnectionMode::Straight, false};
        saved.preferences(application::GraphKey::ScenarioHistory).seriesStyles["hits"] = {
            application::RgbaColor{4, 5, 6, 255}, 9.0};
        saved.preferences(application::GraphKey::BenchmarkRank).seriesStyles["rank"] = {
            application::RgbaColor{7, 8, 9, 255}, std::nullopt};
        saved.preferences(application::GraphKey::BenchmarkPlaytime).appearance.lineModeMarkers = true;
        startupEnv.graphConfigurationPersistence->save(saved);

        {
            application::App restored(startupEnv.settings, std::make_shared<data::ProtoDecoder>(),
                                      startupEnv.graphConfigurationPersistence, startupEnv.statsParser,
                                      startupEnv.benchmarkStore, startupEnv.daySource);

            EXPECT_EQ(probe(restored.graphVm()), (StyleProbe{1, true}));
            EXPECT_EQ(probe(restored.playtimeVm()), (StyleProbe{2, false}));
            EXPECT_EQ(probe(restored.completionHistoryVm()), (StyleProbe{1, false}));
            const auto hits = restored.completionHistoryVm()->series({presentation::CompletionHistoryViewModel::Hits});
            EXPECT_EQ(hits.front()->color(), QColor(4, 5, 6));
            EXPECT_DOUBLE_EQ(hits.front()->lineWidth, 9.0);
            EXPECT_EQ(probe(restored.benchmarkTrackingVm()->rankHistory()), (StyleProbe{0, false}));
            EXPECT_EQ(probe(restored.benchmarkTrackingVm()->playtimeHistory()), (StyleProbe{0, true}));
        }
    }
}
