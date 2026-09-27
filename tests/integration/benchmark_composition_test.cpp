#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QDir>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <map>
#include <optional>
#include <string>

#include "app/app.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmark_breakdown_model.h"
#include "presentation/benchmark_issue_text.h"
#include "presentation/benchmark_manager_vm.h"
#include "presentation/benchmark_table_model.h"
#include "presentation/benchmark_tracking_vm.h"
#include "qt_data/benchmark_store.h"
#include "qt_data/series_config_store.h"
#include "fake_settings_service.h"
#include "integration_env.h"

using namespace ksv;
using namespace ksv::application;
using namespace ksv::data;

namespace {
    QByteArray trackableJson() {
        return R"({
            "schemaVersion": 1, "id": "b1", "name": "Composed",
            "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] } ],
            "uncategorized": [ { "id": "s1", "name": "One", "hash": null,
                "thresholds": [ { "tierId": "t1", "score": 100.0 } ] } ],
            "categories": []
        })";
    }

    QByteArray unmappedJson() {
        return R"({
            "schemaVersion": 1, "id": "auto-1", "name": "Auto",
            "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] } ],
            "uncategorized": [ { "id": "s1", "name": "1wall6targets TE", "hash": null,
                "thresholds": [ { "tierId": "t1", "score": 100.0 } ] } ],
            "categories": []
        })";
    }
}

TEST(BenchmarkComposition, RealGraphAutoResolvesAScenarioNameAtStartup) {
    integration::TestEnv env;
    ASSERT_TRUE(env.valid());
    ASSERT_TRUE(env.makePerformancesDir());
    ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
    QTemporaryDir benchmarks;
    ASSERT_TRUE(benchmarks.isValid());
    QFile file(QDir(benchmarks.path()).absoluteFilePath("auto.json"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(unmappedJson());
    file.close();

    auto repo = std::make_shared<qt_data::BenchmarkStore>(benchmarks.path().toStdString());
    App app(env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore, env.statsParser, repo);
    QElapsedTimer timer;
    timer.start();
    while (!app.profileService()->isProfileLoaded()) {
        ASSERT_LT(timer.elapsed(), 5000);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    const auto resolutionSnapshot = app.benchmarkResolutionUseCase()->snapshot();
    const auto resolutionIt = resolutionSnapshot.resolutions.find(domain::BenchmarkId{"auto-1"});
    ASSERT_NE(resolutionIt, resolutionSnapshot.resolutions.end());
    const auto &resolutions = resolutionIt->second;
    ASSERT_EQ(resolutions.size(), 1U);
    EXPECT_EQ(resolutions[0].state, domain::ScenarioMatchState::Resolved);
    ASSERT_TRUE(resolutions[0].hash.has_value());
    EXPECT_EQ(*resolutions[0].hash, "3e50391f3c3f484c10a4b8fb362ded17");
    const qt_data::BenchmarkStore reread(benchmarks.path().toStdString());
    const auto rescanned = reread.scan();
    ASSERT_TRUE(rescanned.snapshot.has_value());
    const auto *loaded = std::get_if<LoadedBenchmark>(&rescanned.snapshot->entries[0].content);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->benchmark.uncategorized[0].hash,
              std::optional<std::string>{"3e50391f3c3f484c10a4b8fb362ded17"});
}

TEST(BenchmarkComposition, RealGraphProducesProjectionForResolvedBenchmark) {
    integration::TestEnv env;
    ASSERT_TRUE(env.valid());
    ASSERT_TRUE(env.makePerformancesDir());
    ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
    QTemporaryDir benchmarks;
    ASSERT_TRUE(benchmarks.isValid());
    QFile file(QDir(benchmarks.path()).absoluteFilePath("auto.json"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(unmappedJson());
    file.close();

    auto repo = std::make_shared<qt_data::BenchmarkStore>(benchmarks.path().toStdString());
    App app(env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore, env.statsParser, repo);
    QElapsedTimer timer;
    timer.start();
    while (!app.profileService()->isProfileLoaded()) {
        ASSERT_LT(timer.elapsed(), 5000);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    const auto tracking = app.benchmarkTrackingUseCase();
    ASSERT_NE(tracking, nullptr);
    tracking->select(domain::BenchmarkId{"auto-1"});
    ASSERT_EQ(tracking->state(), BenchmarkTrackingState::Ready);
    const auto *projection = tracking->projection();
    ASSERT_NE(projection, nullptr);
    ASSERT_EQ(projection->scenarios.size(), 1U);
    EXPECT_EQ(projection->scenarios[0].matchState, domain::ScenarioMatchState::Resolved);
    EXPECT_TRUE(projection->scenarios[0].personalBest.has_value());
    EXPECT_EQ(projection->scenarios[0].runCount, 1);
    EXPECT_GT(projection->totalPlaytimeSeconds, 0.0);
}

TEST(BenchmarkComposition, RealGraphReportsUnknownBenchmarkAsUnavailable) {
    QTemporaryDir benchmarks;
    ASSERT_TRUE(benchmarks.isValid());
    auto settings = std::make_shared<tests_support::FakeSettingsService>();
    auto repo = std::make_shared<qt_data::BenchmarkStore>(benchmarks.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings),
            std::make_shared<StatsCsvParser>(), repo);
    app.benchmarkTrackingUseCase()->select(domain::BenchmarkId{"missing"});
    EXPECT_EQ(app.benchmarkTrackingUseCase()->state(), BenchmarkTrackingState::Unavailable);
    EXPECT_EQ(app.benchmarkTrackingUseCase()->projection(), nullptr);
}

TEST(BenchmarkComposition, RealGraphLoadsManagedBenchmarksAtStartup) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile file(QDir(dir.path()).absoluteFilePath("composed.json"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(trackableJson());
    file.close();

    auto settings = std::make_shared<tests_support::FakeSettingsService>();
    auto repo = std::make_shared<qt_data::BenchmarkStore>(dir.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings),
            std::make_shared<StatsCsvParser>(), repo);

    const auto service = app.benchmarksService();
    ASSERT_NE(service, nullptr);
    ASSERT_TRUE(service->snapshot().has_value());
    ASSERT_EQ(service->snapshot()->entries.size(), 1U);
    const auto *loaded = std::get_if<LoadedBenchmark>(&service->snapshot()->entries[0].content);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->benchmark.name, "Composed");
}

namespace {
    // A valid, editable document for the given embedded id, used to simulate a hand edit
    // of a managed file on disk.
    QByteArray externallyEditedJson(const std::string &id) {
        return R"({
            "schemaVersion": 1, "id": ")" + QByteArray::fromStdString(id) + R"(", "name": "Externally Edited",
            "tiers": [],
            "uncategorized": [
                { "id": "s1", "name": "One", "hash": null, "thresholds": [] }
            ],
            "categories": []
        })";
    }
}

TEST(BenchmarkComposition, AuthoringRoundTripImportsSavesReopensAndDeletes) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    auto settings = std::make_shared<tests_support::FakeSettingsService>();
    auto repo = std::make_shared<qt_data::BenchmarkStore>(dir.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings),
            std::make_shared<StatsCsvParser>(), repo);

    auto manager = app.benchmarkManagerUseCase();
    ASSERT_NE(manager, nullptr);
    auto service = app.benchmarksService();
    ASSERT_NE(service, nullptr);
    ASSERT_NE(app.benchmarkManagerVm(), nullptr);

    const auto import = manager->importPlaylistSeed(
        QDir(TEST_FILES_DIR).absoluteFilePath("Viscose Benchmarks Beta - Intermediate.json").toStdString());
    ASSERT_TRUE(import.ok());
    EXPECT_TRUE(import.skippedDuplicateIndices.empty());
    ASSERT_TRUE(import.seed.has_value());
    EXPECT_FALSE(import.seed->token.has_value());
    domain::Benchmark working = import.seed->benchmark;
    EXPECT_EQ(working.name, "Viscose Benchmarks Beta - Intermediate");
    ASSERT_EQ(working.uncategorized.size(), 36U);
    EXPECT_EQ(working.uncategorized.front().name, "WhisphereRawControl");
    EXPECT_EQ(working.uncategorized.back().name, "voxTargetClick 20% Small");
    const domain::BenchmarkId benchmarkId = working.id;

    const auto savedPath =
        QDir(dir.path()).absoluteFilePath(QString::fromStdString(benchmarkId.value) + ".json");
    working.name = "Viscose Intermediate";
    const auto save = manager->save(working, import.seed->token);
    ASSERT_TRUE(save.ok());
    ASSERT_TRUE(QFile::exists(savedPath));
    ASSERT_TRUE(service->snapshot().has_value());
    ASSERT_EQ(service->snapshot()->entries.size(), 1U);
    EXPECT_EQ(service->snapshot()->entries.front().filename, benchmarkId.value + ".json");

    manager->closeEditor();
    const auto reopened = manager->openBenchmark(benchmarkId);
    ASSERT_TRUE(reopened.has_value());
    ASSERT_EQ(reopened->benchmark.uncategorized.size(), 36U);
    EXPECT_EQ(reopened->benchmark.uncategorized.front().name, "WhisphereRawControl");
    EXPECT_EQ(reopened->benchmark.uncategorized.back().name, "voxTargetClick 20% Small");
    ASSERT_TRUE(reopened->token.has_value());

    // A hand edit on disk makes the stale-token save a conflict that leaves the file intact.
    {
        QFile file(savedPath);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(externallyEditedJson(benchmarkId.value));
    }
    domain::Benchmark stale = reopened->benchmark;
    stale.name = "Renamed After External Edit";
    const auto conflicted = manager->save(stale, reopened->token);
    EXPECT_EQ(conflicted.error, std::make_optional(BenchmarkSaveError::Conflict));
    {
        QFile file(savedPath);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), externallyEditedJson(benchmarkId.value));
    }

    // Explicit refresh admits the external edit; the reopened definition is then deletable.
    manager->refresh();
    const auto readmitted = manager->openBenchmark(benchmarkId);
    ASSERT_TRUE(readmitted.has_value());
    EXPECT_EQ(readmitted->benchmark.name, "Externally Edited");

    const auto deleted = manager->deleteBenchmark(*readmitted->token);
    ASSERT_TRUE(deleted.ok());
    EXPECT_FALSE(QFile::exists(savedPath));
    ASSERT_TRUE(service->snapshot().has_value());
    EXPECT_TRUE(service->snapshot()->entries.empty());
}

TEST(BenchmarkComposition, RefreshDuringEditPreservesWorkingCopyAndStaleSaveConflicts) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const auto path = QDir(dir.path()).absoluteFilePath("b1.json");
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(trackableJson());
    }

    auto settings = std::make_shared<tests_support::FakeSettingsService>();
    auto repo = std::make_shared<qt_data::BenchmarkStore>(dir.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings),
            std::make_shared<StatsCsvParser>(), repo);

    auto *vm = app.benchmarkManagerVm();
    ASSERT_NE(vm, nullptr);
    auto service = app.benchmarksService();

    ASSERT_TRUE(vm->openBenchmark("b1"));
    vm->setBenchmarkName("Edited locally");
    ASSERT_TRUE(vm->dirty());
    ASSERT_FALSE(vm->baselineStale());

    // A hand edit rewrites the managed file; explicit refresh admits it independently.
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(externallyEditedJson("b1"));
    }
    vm->refresh();

    ASSERT_TRUE(service->snapshot().has_value());
    ASSERT_EQ(service->snapshot()->entries.size(), 1U);
    const auto *refreshed = std::get_if<LoadedBenchmark>(&service->snapshot()->entries.front().content);
    ASSERT_NE(refreshed, nullptr);
    EXPECT_EQ(refreshed->benchmark.name, "Externally Edited");   // library reflects the external file

    EXPECT_EQ(vm->benchmarkName(), "Edited locally");            // working copy unchanged
    EXPECT_TRUE(vm->dirty());
    EXPECT_TRUE(vm->baselineStale());

    const auto result = vm->save();
    EXPECT_FALSE(result["ok"].toBool());                        // real store rejects the stale digest
    EXPECT_FALSE(result["error"].toString().isEmpty());
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), externallyEditedJson("b1"));  // file on disk intact
    }
    const auto *afterConflict =
        std::get_if<LoadedBenchmark>(&service->snapshot()->entries.front().content);
    ASSERT_NE(afterConflict, nullptr);
    EXPECT_EQ(afterConflict->benchmark.name, "Externally Edited");  // accepted memory still the refreshed value
}

TEST(BenchmarkComposition, EditLeaseDefersAndReleasePersistsLatestAutomaticMapping) {
    integration::TestEnv env;
    ASSERT_TRUE(env.valid());
    ASSERT_TRUE(env.makePerformancesDir());

    QTemporaryDir benchmarks;
    ASSERT_TRUE(benchmarks.isValid());
    const auto path = QDir(benchmarks.path()).absoluteFilePath("auto.json");
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(unmappedJson());  // id "auto-1", hashless scenario "1wall6targets TE"
    }

    auto repo = std::make_shared<qt_data::BenchmarkStore>(benchmarks.path().toStdString());
    App app(env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore, env.statsParser, repo);

    QElapsedTimer timer;
    timer.start();
    while (!app.profileService()->isProfileLoaded()) {
        ASSERT_LT(timer.elapsed(), 5000);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    auto service = app.benchmarksService();
    auto manager = app.benchmarkManagerUseCase();
    const auto hashOf = [&](const data::BenchmarkLibrarySnapshot &snapshot) {
        const auto *loaded = std::get_if<LoadedBenchmark>(&snapshot.entries.front().content);
        return loaded ? loaded->benchmark.uncategorized.front().hash : std::optional<std::string>{};
    };

    // No matching perf yet, so nothing auto-maps. Open the benchmark for editing (takes the lease).
    ASSERT_TRUE(service->snapshot().has_value());
    EXPECT_FALSE(hashOf(*service->snapshot()).has_value());
    ASSERT_TRUE(manager->openBenchmark(domain::BenchmarkId{"auto-1"}).has_value());

    // The scenario becomes uniquely resolvable through a profile rebuild.
    ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
    app.profileService()->generateProfileFromDirectory();
    timer.restart();
    while (app.benchmarkResolutionUseCase()->snapshot().scenarioCatalogue.empty()) {
        ASSERT_LT(timer.elapsed(), 5000);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    // The leased benchmark's automatic write is deferred: the file is still hashless.
    {
        qt_data::BenchmarkStore reread(benchmarks.path().toStdString());
        EXPECT_FALSE(hashOf(*reread.scan().snapshot).has_value());
    }
    EXPECT_FALSE(hashOf(*service->snapshot()).has_value());

    // An external refresh admits a new definition/digest while the editor is still open.
    QByteArray refreshed = QByteArray(unmappedJson()).replace(R"("name": "Auto")", R"("name": "Auto v2")");
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(refreshed);
    }
    manager->refresh();

    // Closing the editor releases the lease; reconciliation persists the latest admitted definition.
    manager->closeEditor();
    for (int i = 0; i < 20; ++i) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    qt_data::BenchmarkStore finalStore(benchmarks.path().toStdString());
    const auto finalScan = finalStore.scan();
    ASSERT_TRUE(finalScan.snapshot.has_value());
    ASSERT_EQ(finalScan.snapshot->entries.size(), 1U);
    const auto *finalLoaded =
        std::get_if<LoadedBenchmark>(&finalScan.snapshot->entries.front().content);
    ASSERT_NE(finalLoaded, nullptr);
    EXPECT_EQ(finalLoaded->benchmark.name, "Auto v2");  // reconciled against the refreshed definition
    ASSERT_TRUE(finalLoaded->benchmark.uncategorized.front().hash.has_value());
    EXPECT_EQ(*finalLoaded->benchmark.uncategorized.front().hash, "3e50391f3c3f484c10a4b8fb362ded17");
    EXPECT_EQ(hashOf(*service->snapshot()), std::optional<std::string>{std::string{"3e50391f3c3f484c10a4b8fb362ded17"}});
}

// --- Task I1: end-to-end production wiring and the manager -> tracking publication cascade -------

namespace {
    const char *kWallSixHash = "3e50391f3c3f484c10a4b8fb362ded17";

    // One trackable definition whose only scenario is hash-mapped to the "1wall6targets TE" perf
    // and sits inside a category/subcategory, so a select() exercises ranks, history, playtime,
    // recent averages and the nested hierarchy at once.
    QByteArray hierarchyTrackableJson() {
        return R"({
            "schemaVersion": 1, "id": "b-track", "name": "Tracked",
            "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] } ],
            "uncategorized": [],
            "categories": [
                { "id": "c1", "name": "Aim", "color": [10,20,30,255], "scenarios": [],
                  "subcategories": [
                    { "id": "sc1", "name": "Clicking", "color": [40,50,60,255], "scenarios": [
                        { "id": "s1", "name": "Wall Six", "hash": ")" + QByteArray(kWallSixHash) + R"(",
                          "thresholds": [ { "tierId": "t1", "score": 1.0 } ] }
                    ] }
                  ] }
            ]
        })";
    }

    // Structurally complete, but its single scenario is unmapped and its name does not match the
    // perf, so startup auto-resolution leaves it Unresolved until a manager mapping edit is saved.
    QByteArray unresolvedTrackableJson() {
        return R"({
            "schemaVersion": 1, "id": "b-cascade", "name": "Cascade",
            "tiers": [ { "id": "t1", "name": "Bronze", "color": [200,120,0,255] } ],
            "uncategorized": [
                { "id": "s1", "name": "Needs Mapping", "hash": null,
                  "thresholds": [ { "tierId": "t1", "score": 1.0 } ] }
            ],
            "categories": []
        })";
    }

    struct WiredWorkspace {
        integration::TestEnv env;
        std::shared_ptr<qt_data::BenchmarkStore> repo;
        std::unique_ptr<App> app;
    };

    // A real App over a temp KovaaKs dir holding the "1wall6targets TE" perf (an empty one when
    // `seedPerf` is false) and a temp benchmarks dir holding `benchmarkJson`, run until the profile
    // has loaded.
    std::unique_ptr<WiredWorkspace> wireWorkspace(QTemporaryDir &benchmarks, const QByteArray &benchmarkJson,
                                                  bool seedPerf = true) {
        auto w = std::make_unique<WiredWorkspace>();
        if (!w->env.valid() || !w->env.makePerformancesDir()) return nullptr;
        if (seedPerf && w->env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty()) return nullptr;
        if (!benchmarks.isValid()) return nullptr;
        QFile file(QDir(benchmarks.path()).absoluteFilePath("bench.json"));
        if (!file.open(QIODevice::WriteOnly)) return nullptr;
        file.write(benchmarkJson);
        file.close();

        w->repo = std::make_shared<qt_data::BenchmarkStore>(benchmarks.path().toStdString());
        w->app = std::make_unique<App>(w->env.settings, std::make_shared<data::ProtoDecoder>(),
                                       w->env.seriesConfigStore, w->env.statsParser, w->repo);
        QElapsedTimer timer;
        timer.start();
        while (!w->app->profileService()->isProfileLoaded()) {
            if (timer.elapsed() >= 5000) return nullptr;
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        return w;
    }

    bool mainQmlRequires(const QString &propertyName) {
        QFile file(QStringLiteral(KSV_UI_QML_DIR "/Main.qml"));
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QString source = QString::fromUtf8(file.readAll());
        const QRegularExpression declaration(
            QStringLiteral("required\\s+property\\s+[\\w.<>]+\\s+%1\\b").arg(propertyName));
        return declaration.match(source).hasMatch();
    }

    QModelIndex findBreakdownNode(const QAbstractItemModel *model, const QString &name,
                                  const QModelIndex &parent = {}) {
        for (int row = 0; row < model->rowCount(parent); ++row) {
            const QModelIndex idx = model->index(row, 0, parent);
            if (idx.data(presentation::BenchmarkBreakdownModel::NameRole).toString() == name) return idx;
            if (const QModelIndex hit = findBreakdownNode(model, name, idx); hit.isValid()) return hit;
        }
        return {};
    }
}

TEST(BenchmarkWorkspaceComposition, AppConstructsBothUseCasesAndBothViewModels) {
    QTemporaryDir benchmarks;
    auto w = wireWorkspace(benchmarks, hierarchyTrackableJson());
    ASSERT_NE(w, nullptr);

    EXPECT_NE(w->app->benchmarkTrackingUseCase(), nullptr);
    EXPECT_NE(w->app->benchmarkManagerUseCase(), nullptr);
    EXPECT_NE(w->app->benchmarkTrackingVm(), nullptr);
    EXPECT_NE(w->app->benchmarkManagerVm(), nullptr);
    EXPECT_NE(w->app->benchmarksService(), nullptr);
    EXPECT_NE(w->app->benchmarkResolutionUseCase(), nullptr);
}

TEST(BenchmarkWorkspaceComposition, StartSuppliesBothBenchmarkViewModelsAsRequiredMainProperties) {
    integration::TestEnv env;
    ASSERT_TRUE(env.valid());

    App app(env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore, env.statsParser,
            env.benchmarkStore);
    ASSERT_EQ(app.start(), 0) << "Main.qml failed to load";

    ASSERT_FALSE(app.engine()->rootObjects().isEmpty());
    auto *root = app.engine()->rootObjects().first();

    EXPECT_TRUE(mainQmlRequires(QStringLiteral("benchmarkTrackingVm")))
        << "Main.qml must declare benchmarkTrackingVm as a required property";
    EXPECT_TRUE(mainQmlRequires(QStringLiteral("benchmarkManagerVm")))
        << "Main.qml must declare benchmarkManagerVm as a required property";

    EXPECT_EQ(root->property("benchmarkTrackingVm").value<QObject *>(),
              static_cast<QObject *>(app.benchmarkTrackingVm()));
    EXPECT_EQ(root->property("benchmarkManagerVm").value<QObject *>(),
              static_cast<QObject *>(app.benchmarkManagerVm()));
}

TEST(BenchmarkWorkspaceComposition, SelectingLoadedBenchmarkThroughTrackingViewModelReachesPresentation) {
    QTemporaryDir benchmarks;
    auto w = wireWorkspace(benchmarks, hierarchyTrackableJson());
    ASSERT_NE(w, nullptr);

    auto *vm = w->app->benchmarkTrackingVm();
    QSignalSpy changed(vm, &presentation::BenchmarkTrackingViewModel::changed);
    vm->selectBenchmark(QStringLiteral("b-track"));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    EXPECT_GE(changed.count(), 1);
    EXPECT_EQ(vm->state(), QStringLiteral("trackable"));
    EXPECT_TRUE(vm->summaryAvailable());

    const QString unavailable = QObject::tr("Unavailable");
    EXPECT_NE(vm->attainedRank(), unavailable);
    EXPECT_NE(vm->completedRank(), unavailable);
    EXPECT_NE(vm->averageRank(), unavailable);
    EXPECT_FALSE(vm->totalPlaytime().isEmpty());
    EXPECT_NE(vm->nextTier(), unavailable);

    EXPECT_NE(vm->rankHistory(), nullptr);
    EXPECT_NE(vm->playtimeHistory(), nullptr);
    ASSERT_NE(vm->breakdown(), nullptr);
    EXPECT_GT(vm->breakdown()->rowCount({}), 0);
    EXPECT_TRUE(vm->playtimeHistory()->hasData());

    const QModelIndex scenario = findBreakdownNode(vm->breakdown(), QStringLiteral("Wall Six"));
    ASSERT_TRUE(scenario.isValid()) << "breakdown hierarchy never surfaced the mapped scenario";
    EXPECT_EQ(scenario.data(presentation::BenchmarkBreakdownModel::MatchStateTextRole).toString(),
              QStringLiteral("Resolved"));
    EXPECT_FALSE(scenario.data(presentation::BenchmarkBreakdownModel::PersonalBestTextRole)
                     .toString().isEmpty());

    // The application projection carries the numeric detail the Qt surface adapts.
    const auto *projection = w->app->benchmarkTrackingUseCase()->projection();
    ASSERT_NE(projection, nullptr);
    ASSERT_EQ(projection->scenarios.size(), 1U);
    EXPECT_EQ(projection->scenarios[0].matchState, domain::ScenarioMatchState::Resolved);
    EXPECT_TRUE(projection->scenarios[0].personalBest.has_value());
    EXPECT_TRUE(projection->scenarios[0].recentAverage.has_value());
    EXPECT_EQ(projection->scenarios[0].runCount, 1);
    EXPECT_GT(projection->totalPlaytimeSeconds, 0.0);
    EXPECT_FALSE(projection->rollingPlaytime.empty());
    EXPECT_FALSE(projection->averageRankHistory.empty());
    EXPECT_TRUE(projection->attainedRank.has_value());
    EXPECT_EQ(projection->categories.size(), 1U);
}

TEST(BenchmarkWorkspaceComposition, ManagerMappingSaveRefreshesSelectedTrackingWorkspaceWithoutReselection) {
    QTemporaryDir benchmarks;
    auto w = wireWorkspace(benchmarks, unresolvedTrackableJson());
    ASSERT_NE(w, nullptr);

    auto *vm = w->app->benchmarkTrackingVm();
    vm->selectBenchmark(QStringLiteral("b-cascade"));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    const auto *before = w->app->benchmarkTrackingUseCase()->projection();
    ASSERT_NE(before, nullptr);
    ASSERT_EQ(before->scenarios.size(), 1U);
    ASSERT_EQ(before->scenarios[0].matchState, domain::ScenarioMatchState::Unresolved);
    ASSERT_FALSE(before->scenarios[0].personalBest.has_value());

    const QModelIndex unmapped = findBreakdownNode(vm->breakdown(), QStringLiteral("Needs Mapping"));
    ASSERT_TRUE(unmapped.isValid());
    EXPECT_EQ(unmapped.data(presentation::BenchmarkBreakdownModel::MatchStateTextRole).toString(),
              QStringLiteral("Unresolved"));

    QSignalSpy changed(vm, &presentation::BenchmarkTrackingViewModel::changed);

    // Map the scenario and persist it through the manager use case; do NOT touch the tracking VM.
    auto manager = w->app->benchmarkManagerUseCase();
    const auto seed = manager->openBenchmark(domain::BenchmarkId{"b-cascade"});
    ASSERT_TRUE(seed.has_value());
    domain::Benchmark mapped = seed->benchmark;
    ASSERT_EQ(mapped.uncategorized.size(), 1U);
    mapped.uncategorized[0].hash = std::optional<std::string>{std::string{"3e50391f3c3f484c10a4b8fb362ded17"}};
    ASSERT_TRUE(manager->save(mapped, seed->token).ok());
    manager->closeEditor();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    EXPECT_GE(changed.count(), 1) << "the manager save never republished the tracking workspace";

    const auto *after = w->app->benchmarkTrackingUseCase()->projection();
    ASSERT_NE(after, nullptr);
    ASSERT_EQ(after->scenarios.size(), 1U);
    EXPECT_EQ(after->scenarios[0].matchState, domain::ScenarioMatchState::Resolved);
    EXPECT_TRUE(after->scenarios[0].personalBest.has_value());
    EXPECT_EQ(after->scenarios[0].runCount, 1);
    EXPECT_GT(after->totalPlaytimeSeconds, 0.0);

    EXPECT_EQ(vm->attainedRank(), QStringLiteral("Bronze"));
    EXPECT_TRUE(vm->blockers().isEmpty());
    const QModelIndex remapped = findBreakdownNode(vm->breakdown(), QStringLiteral("Needs Mapping"));
    ASSERT_TRUE(remapped.isValid());
    EXPECT_EQ(remapped.data(presentation::BenchmarkBreakdownModel::MatchStateTextRole).toString(),
              QStringLiteral("Resolved"));
    EXPECT_NE(remapped.data(presentation::BenchmarkBreakdownModel::PersonalBestTextRole).toString(),
              QStringLiteral("Unplayed"));
}

// --- Benchmark table editor: real composition ----------------------------------------------------

namespace {
    using TableRole = presentation::BenchmarkTableModel::Role;

    class DefaultLocaleGuard {
    public:
        explicit DefaultLocaleGuard(const QLocale &locale) { QLocale::setDefault(locale); }
        ~DefaultLocaleGuard() { QLocale::setDefault(m_previous); }
        DefaultLocaleGuard(const DefaultLocaleGuard &) = delete;
        DefaultLocaleGuard &operator=(const DefaultLocaleGuard &) = delete;

    private:
        QLocale m_previous;
    };

    const QLocale kEnglishGb{QLocale::English, QLocale::UnitedKingdom};

    QVariant tableCell(presentation::BenchmarkManagerViewModel &vm, const QString &entryId, const QString &tierId,
                       int role) {
        auto *model = qobject_cast<presentation::BenchmarkTableModel *>(vm.tableModel());
        const int row = model->rowForEntry(entryId);
        const int column = tierId.isEmpty() ? 2 : model->columnForTier(tierId);
        return model->data(model->index(row, column), role);
    }

    QStringList tableRowIds(presentation::BenchmarkManagerViewModel &vm) {
        QStringList ids;
        auto *model = vm.tableModel();
        for (int row = 0; row < model->rowCount(); ++row)
            ids.push_back(model->data(model->index(row, 2), TableRole::EntryIdRole).toString());
        return ids;
    }

    QVariantMap at(const QString &kind, const QString &entryId = {}, const QString &tierId = {}) {
        return QVariantMap{{"kind", kind}, {"entryId", entryId}, {"tierId", tierId}};
    }

    QJsonObject readJson(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    QJsonObject jsonEntry(const QJsonObject &root, const QString &id) {
        const auto match = [&](const QJsonArray &entries) -> QJsonObject {
            for (const auto &entry: entries)
                if (entry.toObject().value("id").toString() == id) return entry.toObject();
            return {};
        };
        if (auto found = match(root.value("uncategorized").toArray()); !found.isEmpty()) return found;
        for (const auto &category: root.value("categories").toArray()) {
            if (auto found = match(category.toObject().value("scenarios").toArray()); !found.isEmpty()) return found;
            for (const auto &sub: category.toObject().value("subcategories").toArray())
                if (auto found = match(sub.toObject().value("scenarios").toArray()); !found.isEmpty()) return found;
        }
        return {};
    }

    // tierId -> score for every stored threshold object; each must be exactly {tierId, number}.
    std::map<QString, double> storedScores(const QJsonObject &entry) {
        std::map<QString, double> scores;
        for (const auto &threshold: entry.value("thresholds").toArray()) {
            const auto object = threshold.toObject();
            EXPECT_EQ(object.keys(), (QStringList{"score", "tierId"})) << "no provenance or extra fields";
            EXPECT_TRUE(object.value("score").isDouble()) << "a stored score is always a number";
            scores[object.value("tierId").toString()] = object.value("score").toDouble();
        }
        return scores;
    }

    QString appendedTierIds(const presentation::BenchmarkManagerViewModel &vm, int index) {
        return vm.tiers().at(index).toMap().value("id").toString();
    }
}

TEST(BenchmarkTableComposition, MalformedSaveRoundTrip) {
    const DefaultLocaleGuard guard{kEnglishGb};
    QTemporaryDir benchmarks;
    auto w = wireWorkspace(benchmarks, hierarchyTrackableJson());
    ASSERT_NE(w, nullptr);
    auto *vm = w->app->benchmarkManagerVm();
    const auto path = QDir(benchmarks.path()).absoluteFilePath("bench.json");
    const auto schemaBefore = readJson(path).value("schemaVersion");

    ASSERT_TRUE(vm->openBenchmark("b-track"));
    ASSERT_TRUE(vm->pasteText(at("rankHeader"), "Silver\tGold\tPlatinum\tDiamond")["ok"].toBool());
    ASSERT_EQ(vm->tiers().size(), 5);
    QStringList tiers;
    for (int i = 0; i < 5; ++i) tiers.push_back(appendedTierIds(*vm, i));
    ASSERT_EQ(tiers.front(), "t1");

    // s1: finite 1, zero, a finite negative that also breaks the increasing order, then two invalid.
    ASSERT_TRUE(vm->pasteText(at("threshold", "s1", "t1"), "1\t0\t-5\toops\tNaN")["ok"].toBool());
    // An appended unnamed row: finite neighbours around non-finite spellings.
    ASSERT_TRUE(vm->pasteText(at("append"), "\t2\tinfinity\t1e9999\t4\t5")["ok"].toBool());
    const QStringList rows = tableRowIds(*vm);
    ASSERT_EQ(rows.size(), 2);
    const QString unnamed = rows.at(1);
    ASSERT_TRUE(vm->canUndo());
    ASSERT_EQ(vm->normalizationIssues().size(), 4);

    const auto result = vm->save();
    ASSERT_TRUE(result["ok"].toBool()) << result["error"].toString().toStdString();

    // Stored JSON: invalid inputs are simply absent thresholds; everything finite survives as typed.
    const auto stored = readJson(path);
    EXPECT_EQ(stored.value("schemaVersion"), schemaBefore);
    EXPECT_EQ(stored.value("id").toString(), "b-track");
    const auto s1 = jsonEntry(stored, "s1");
    EXPECT_EQ(s1.value("name").toString(), "Wall Six");
    EXPECT_EQ(s1.value("hash").toString(), kWallSixHash);
    const auto s1Scores = storedScores(s1);
    EXPECT_EQ(s1Scores, (std::map<QString, double>{{tiers[0], 1.0}, {tiers[1], 0.0}, {tiers[2], -5.0}}));
    const auto extra = jsonEntry(stored, unnamed);
    ASSERT_FALSE(extra.isEmpty()) << "the unnamed expansion row is saved with its generated id";
    EXPECT_EQ(extra.value("name").toString(), QString()) << "no placeholder text is stored as a name";
    EXPECT_EQ(storedScores(extra), (std::map<QString, double>{{tiers[0], 2.0}, {tiers[3], 4.0}, {tiers[4], 5.0}}));
    const auto storedTiers = stored.value("tiers").toArray();
    ASSERT_EQ(storedTiers.size(), 5);
    EXPECT_EQ(storedTiers.at(1).toObject().value("name").toString(), "Silver");
    const auto category = stored.value("categories").toArray().at(0).toObject();
    EXPECT_EQ(category.value("id").toString(), "c1");
    EXPECT_EQ(category.value("subcategories").toArray().at(0).toObject().value("id").toString(), "sc1");

    // Accepted publication carries the same normalized definition.
    const auto snapshot = w->app->benchmarksService()->snapshot();
    ASSERT_TRUE(snapshot.has_value());
    const auto *loaded = std::get_if<LoadedBenchmark>(&snapshot->entries.front().content);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->completeness.completeness, domain::Completeness::Incomplete);

    // The session adopted the saved candidate: raw input and Undo are gone, cells read as missing.
    EXPECT_FALSE(vm->canUndo());
    EXPECT_FALSE(vm->dirty());
    EXPECT_TRUE(vm->normalizationIssues().isEmpty());
    const auto missing = presentation::benchmarkIssueText(domain::BenchmarkIssueCode::MissingThreshold);
    EXPECT_EQ(tableCell(*vm, "s1", tiers[3], TableRole::InputStateRole).toString(), QString());
    EXPECT_FALSE(tableCell(*vm, "s1", tiers[3], TableRole::HasValueRole).toBool());
    EXPECT_TRUE(tableCell(*vm, "s1", tiers[3], TableRole::IssuesRole).toStringList().contains(missing));

    // Reopening from accepted data shows the same incomplete definition.
    vm->discard();
    ASSERT_TRUE(vm->openBenchmark("b-track"));
    EXPECT_FALSE(vm->draftTrackable());
    EXPECT_FALSE(tableCell(*vm, unnamed, tiers[1], TableRole::HasValueRole).toBool());
    EXPECT_EQ(tableCell(*vm, unnamed, tiers[3], TableRole::DisplayTextRole).toString(), "4");
    EXPECT_FALSE(vm->canUndo());
    vm->discard();

    // Tracking treats it as incomplete: no official rank, but the scenario facts remain.
    auto *tracking = w->app->benchmarkTrackingVm();
    tracking->selectBenchmark(QStringLiteral("b-track"));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    EXPECT_EQ(tracking->state(), QStringLiteral("incomplete"));
    EXPECT_EQ(tracking->attainedRank(), QObject::tr("Unavailable"));
    const auto *projection = w->app->benchmarkTrackingUseCase()->projection();
    ASSERT_NE(projection, nullptr);
    EXPECT_FALSE(projection->attainedRank.has_value());
    const auto wallSix = std::ranges::find_if(projection->scenarios, [](const auto &scenario) {
        return scenario.entryId.value == "s1";
    });
    ASSERT_NE(wallSix, projection->scenarios.end());
    EXPECT_EQ(wallSix->matchState, domain::ScenarioMatchState::Resolved);
    EXPECT_TRUE(wallSix->personalBest.has_value()) << "the mapped scenario keeps its played facts";
}

TEST(BenchmarkTableComposition, ConflictAndFailedWriteKeepSession) {
    const DefaultLocaleGuard guard{kEnglishGb};
    QTemporaryDir benchmarks;
    auto w = wireWorkspace(benchmarks, hierarchyTrackableJson());
    ASSERT_NE(w, nullptr);
    auto *vm = w->app->benchmarkManagerVm();
    auto service = w->app->benchmarksService();
    const auto path = QDir(benchmarks.path()).absoluteFilePath("bench.json");

    // --- Conflict: the accepted file changed on disk behind the open editor.
    ASSERT_TRUE(vm->openBenchmark("b-track"));
    ASSERT_TRUE(vm->editThresholdText("s1", "t1", "oops")["ok"].toBool());
    ASSERT_TRUE(vm->renameScenario("s1", "Renamed")["ok"].toBool());
    vm->setCurrentCell(QVariantMap{{"entryId", "s1"}, {"tierId", "t1"}});
    const QByteArray external = externallyEditedJson("b-track");
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(external);
    }

    EXPECT_FALSE(vm->save()["ok"].toBool());
    for (int i = 0; i < 5; ++i) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), external) << "a conflicting save never overwrites the external edit";
    }
    const auto *accepted = std::get_if<LoadedBenchmark>(&service->snapshot()->entries.front().content);
    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(accepted->benchmark.uncategorized.size() + accepted->benchmark.categories.size(), 1U);
    EXPECT_EQ(accepted->benchmark.name, "Tracked") << "the accepted snapshot is the one before the attempt";
    EXPECT_EQ(tableCell(*vm, "s1", "t1", TableRole::DisplayTextRole).toString(), "oops");
    EXPECT_EQ(tableCell(*vm, "s1", {}, TableRole::DisplayTextRole).toString(), "Renamed");
    EXPECT_EQ(vm->currentCell().value("entryId").toString(), "s1");
    EXPECT_TRUE(vm->dirty());
    ASSERT_TRUE(vm->canUndo());
    ASSERT_TRUE(vm->undo()["ok"].toBool());
    EXPECT_EQ(tableCell(*vm, "s1", {}, TableRole::DisplayTextRole).toString(), "Wall Six");
    EXPECT_EQ(tableCell(*vm, "s1", "t1", TableRole::DisplayTextRole).toString(), "oops");

    // Discard, explicit refresh and reopen take the accepted (external) definition.
    vm->discard();
    vm->refresh();
    ASSERT_TRUE(vm->openBenchmark("b-track"));
    EXPECT_EQ(vm->benchmarkName(), "Externally Edited");
    EXPECT_FALSE(vm->canUndo());
    vm->discard();

    // --- Write failure: a never-saved draft whose file name is occupied by a directory.
    const auto entriesBefore = service->snapshot()->entries.size();
    vm->beginNewBenchmark();
    vm->setBenchmarkName("Fresh");
    ASSERT_TRUE(vm->pasteText(at("append"), "Fresh scenario\t10\tbad")["ok"].toBool());
    const QString freshId = vm->draftId();
    const QString freshEntry = tableRowIds(*vm).front();
    const QString secondTier = appendedTierIds(*vm, 1);
    ASSERT_TRUE(QDir(benchmarks.path()).mkdir(freshId + ".json"));

    EXPECT_FALSE(vm->save()["ok"].toBool());
    for (int i = 0; i < 5; ++i) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    EXPECT_EQ(service->snapshot()->entries.size(), entriesBefore) << "no accepted entry is invented";
    EXPECT_EQ(tableCell(*vm, freshEntry, secondTier, TableRole::DisplayTextRole).toString(), "bad");
    EXPECT_TRUE(vm->dirty());
    ASSERT_TRUE(vm->canUndo());
    ASSERT_TRUE(vm->undo()["ok"].toBool());
    EXPECT_EQ(vm->tableModel()->rowCount(), 0) << "Undo still reverses the paste after the failed write";
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), external) << "unrelated accepted files are untouched";
    }

    vm->discard();
    EXPECT_FALSE(vm->hasDraft());
    EXPECT_EQ(service->snapshot()->entries.size(), entriesBefore);
}

TEST(BenchmarkTableComposition, GroupedPlaylistMappingAndUndo) {
    const DefaultLocaleGuard guard{kEnglishGb};
    QTemporaryDir benchmarks;
    auto w = wireWorkspace(benchmarks, hierarchyTrackableJson());
    ASSERT_NE(w, nullptr);
    auto *vm = w->app->benchmarkManagerVm();
    auto service = w->app->benchmarksService();

    const auto imported = vm->importPlaylist(
        QUrl::fromLocalFile(QDir(TEST_FILES_DIR).absoluteFilePath("Viscose Benchmarks Beta - Intermediate.json")));
    ASSERT_TRUE(imported["ok"].toBool());
    const QStringList seeded = tableRowIds(*vm);
    ASSERT_EQ(seeded.size(), 36);
    vm->setBenchmarkName("Grouped Import");
    const QString tier = vm->addTier("Bronze")["createdId"].toString();
    ASSERT_FALSE(tier.isEmpty());

    // Expand, then group two rows chosen out of visible order, then reorder inside the group.
    ASSERT_TRUE(vm->pasteText(at("append"), "Extra")["ok"].toBool());
    ASSERT_EQ(tableRowIds(*vm).size(), 37);
    const QString category = vm->addCategory("Clicking")["createdId"].toString();
    const QString first = seeded.at(1);
    const QString third = seeded.at(3);
    ASSERT_TRUE(vm->assignScenarios({third, first}, category)["ok"].toBool());
    EXPECT_EQ(tableRowIds(*vm).mid(0, 2), (QStringList{first, third})) << "grouping keeps visible order";
    ASSERT_TRUE(vm->reorderScenario(third, 0)["ok"].toBool());
    EXPECT_EQ(tableRowIds(*vm).mid(0, 2), (QStringList{third, first}));

    // Paste lands by the new visible order.
    ASSERT_TRUE(vm->pasteText(at("threshold", third, tier), "10\n20")["ok"].toBool());
    EXPECT_EQ(tableCell(*vm, third, tier, TableRole::DisplayTextRole).toString(), "10");
    EXPECT_EQ(tableCell(*vm, first, tier, TableRole::DisplayTextRole).toString(), "20");

    // A mapping edit resolves against the live profile, and Undo re-resolves after reverting it.
    ASSERT_TRUE(vm->setScenarioHash(first, kWallSixHash)["ok"].toBool());
    EXPECT_EQ(tableCell(*vm, first, {}, TableRole::MappingStateRole).toString(), "resolved");
    ASSERT_TRUE(vm->undo()["ok"].toBool());
    EXPECT_NE(tableCell(*vm, first, {}, TableRole::MappingStateRole).toString(), "resolved");
    EXPECT_EQ(tableCell(*vm, first, tier, TableRole::DisplayTextRole).toString(), "20");
    ASSERT_TRUE(vm->setScenarioHash(first, kWallSixHash)["ok"].toBool());

    // Nothing reaches the library while the draft is being edited.
    EXPECT_EQ(service->snapshot()->entries.size(), 1U);

    ASSERT_TRUE(vm->save()["ok"].toBool());
    const QString id = vm->draftId();
    EXPECT_FALSE(vm->canUndo());
    vm->discard();
    ASSERT_TRUE(vm->openBenchmark(id));
    EXPECT_EQ(tableRowIds(*vm).mid(0, 2), (QStringList{third, first}));
    EXPECT_EQ(tableCell(*vm, third, tier, TableRole::DisplayTextRole).toString(), "10");
    EXPECT_EQ(tableCell(*vm, first, tier, TableRole::DisplayTextRole).toString(), "20");
    EXPECT_EQ(tableCell(*vm, first, {}, TableRole::MappingStateRole).toString(), "resolved");
    EXPECT_FALSE(vm->canUndo()) << "history is session-only";
    EXPECT_TRUE(vm->normalizationIssues().isEmpty()) << "no raw input is persisted";

    const auto stored = readJson(QDir(benchmarks.path()).absoluteFilePath(id + ".json"));
    EXPECT_EQ(jsonEntry(stored, first).value("hash").toString(), kWallSixHash);
}

// A profile rebuild that makes a leased draft's hashless scenario resolvable must show in the table
// at once, before any edit or Undo, while the local session and the stored file stay untouched.
TEST(BenchmarkTableComposition, ProfileRebuildRefreshesLeasedDraftMapping) {
    const DefaultLocaleGuard guard{kEnglishGb};
    QTemporaryDir benchmarks;
    auto w = wireWorkspace(benchmarks, unmappedJson(), false);
    ASSERT_NE(w, nullptr);
    auto *vm = w->app->benchmarkManagerVm();
    ASSERT_TRUE(vm->openBenchmark("auto-1"));
    ASSERT_EQ(tableCell(*vm, "s1", {}, TableRole::MappingStateRole).toString(), "unresolved");
    ASSERT_TRUE(vm->editThresholdText("s1", "t1", "123")["ok"].toBool());
    ASSERT_TRUE(vm->addTier("Silver")["ok"].toBool());
    const auto extraTier = appendedTierIds(*vm, 1);
    ASSERT_TRUE(vm->editThresholdText("s1", extraTier, "oops")["ok"].toBool());
    vm->setSelectedEntryIds({"s1"});
    const QVariantMap current{{"entryId", "s1"}, {"tierId", extraTier}, {"columnKind", 3}};
    vm->setCurrentCell(current);

    const auto path = QDir(benchmarks.path()).absoluteFilePath("bench.json");
    const auto readBytes = [&] {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    const auto accepted = [&] {
        const auto snapshot = w->app->benchmarksService()->snapshot();
        const auto &entry = snapshot->entries.front();
        return std::pair{entry.digest, std::get<LoadedBenchmark>(entry.content).benchmark};
    };
    const QByteArray bytesBefore = readBytes();
    ASSERT_FALSE(bytesBefore.isEmpty());
    const auto acceptedBefore = accepted();

    ASSERT_FALSE(w->env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
    w->app->profileService()->generateProfileFromDirectory();
    QElapsedTimer timer;
    timer.start();
    while (w->app->benchmarkResolutionUseCase()->snapshot().scenarioCatalogue.empty()) {
        ASSERT_LT(timer.elapsed(), 5000);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    // No edit or Undo has run since the rebuild.
    EXPECT_NE(tableCell(*vm, "s1", {}, TableRole::MappingStateRole).toString(), "unresolved");
    EXPECT_EQ(tableCell(*vm, "s1", "t1", TableRole::DisplayTextRole).toString(), "123");
    EXPECT_EQ(tableCell(*vm, "s1", extraTier, TableRole::EditTextRole).toString(), "oops");
    EXPECT_EQ(tableCell(*vm, "s1", extraTier, TableRole::InputStateRole).toString(), "invalid");
    EXPECT_TRUE(vm->dirty());
    EXPECT_TRUE(vm->canUndo());
    EXPECT_EQ(vm->selectedEntryIds(), QStringList{"s1"});
    EXPECT_EQ(vm->currentCell(), current);
    EXPECT_EQ(readBytes(), bytesBefore) << "the edit lease keeps the automatic mapping out of the file";
    EXPECT_EQ(accepted(), acceptedBefore);

    // The earlier local edit is still recoverable, and recomputes against the latest profile.
    ASSERT_TRUE(vm->undo()["ok"].toBool());
    EXPECT_EQ(tableCell(*vm, "s1", extraTier, TableRole::InputStateRole).toString(), QString());
    EXPECT_EQ(tableCell(*vm, "s1", "t1", TableRole::DisplayTextRole).toString(), "123");
    EXPECT_NE(tableCell(*vm, "s1", {}, TableRole::MappingStateRole).toString(), "unresolved");
}
