#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QDir>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariant>

#include <optional>
#include <string>

#include "app/app.h"
#include "formats/protobuf/proto_decoder.h"
#include "presentation/benchmark_breakdown_model.h"
#include "presentation/benchmark_tracking_vm.h"
#include "qt_data/benchmark_repository.h"
#include "qt_data/series_config_store.h"
#include "fake_settings_service.h"
#include "integration_env.h"

using namespace ksv;
using namespace ksv::application;

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

    auto repo = std::make_shared<qt_data::BenchmarkRepository>(benchmarks.path().toStdString());
    App app(env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore, nullptr, repo);
    QElapsedTimer timer;
    timer.start();
    while (!app.profileService()->isProfileLoaded()) {
        ASSERT_LT(timer.elapsed(), 5000);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    const auto resolutions = app.benchmarkLibraryService()->resolutionsFor(domain::BenchmarkId{"auto-1"});
    ASSERT_EQ(resolutions.size(), 1U);
    EXPECT_EQ(resolutions[0].state, domain::ScenarioMatchState::Resolved);
    ASSERT_TRUE(resolutions[0].hash.has_value());
    EXPECT_EQ(*resolutions[0].hash, "3e50391f3c3f484c10a4b8fb362ded17");
    const qt_data::BenchmarkRepository reread(benchmarks.path().toStdString());
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

    auto repo = std::make_shared<qt_data::BenchmarkRepository>(benchmarks.path().toStdString());
    App app(env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore, nullptr, repo);
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
    auto repo = std::make_shared<qt_data::BenchmarkRepository>(benchmarks.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings), nullptr, repo);
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
    auto repo = std::make_shared<qt_data::BenchmarkRepository>(dir.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings), nullptr, repo);

    const auto service = app.benchmarkLibraryService();
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
    auto repo = std::make_shared<qt_data::BenchmarkRepository>(dir.path().toStdString());
    App app(settings, std::make_shared<data::ProtoDecoder>(),
            std::make_shared<qt_data::SeriesConfigStore>(settings), nullptr, repo);

    auto service = app.benchmarkLibraryService();
    ASSERT_NE(service, nullptr);
    ASSERT_NE(app.benchmarkManagerVm(), nullptr);

    const auto import = service->importPlaylist(
        QDir(TEST_FILES_DIR).absoluteFilePath("Viscose Benchmarks Beta - Intermediate.json").toStdString());
    ASSERT_TRUE(import.ok());
    EXPECT_TRUE(import.skippedDuplicateIndices.empty());
    ASSERT_TRUE(service->hasDraft());
    ASSERT_TRUE(service->draftDirty());
    const auto draft = service->draft();
    ASSERT_TRUE(draft.has_value());
    EXPECT_EQ(draft->name, "Viscose Benchmarks Beta - Intermediate");
    ASSERT_EQ(draft->uncategorized.size(), 36U);
    EXPECT_EQ(draft->uncategorized.front().name, "WhisphereRawControl");
    EXPECT_EQ(draft->uncategorized.back().name, "voxTargetClick 20% Small");
    const domain::BenchmarkId benchmarkId = draft->id;

    const auto savedPath =
        QDir(dir.path()).absoluteFilePath(QString::fromStdString(benchmarkId.value) + ".json");
    ASSERT_TRUE(service->renameBenchmark("Viscose Intermediate").ok());
    const auto save = service->saveDraft();
    ASSERT_TRUE(save.ok());
    ASSERT_TRUE(QFile::exists(savedPath));
    ASSERT_TRUE(service->snapshot().has_value());
    ASSERT_EQ(service->snapshot()->entries.size(), 1U);
    EXPECT_EQ(service->snapshot()->entries.front().filename,
              benchmarkId.value + ".json");

    service->discardDraft();
    ASSERT_TRUE(service->openDraft(benchmarkId));
    const auto reopened = service->draft();
    ASSERT_TRUE(reopened.has_value());
    ASSERT_EQ(reopened->uncategorized.size(), 36U);
    EXPECT_EQ(reopened->uncategorized.front().name, "WhisphereRawControl");
    EXPECT_EQ(reopened->uncategorized.back().name, "voxTargetClick 20% Small");
    EXPECT_FALSE(service->draftDirty());

    // A hand edit on disk makes the stale-digest save a conflict that leaves the file intact.
    {
        QFile file(savedPath);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(externallyEditedJson(benchmarkId.value));
    }
    ASSERT_TRUE(service->renameBenchmark("Renamed After External Edit").ok());
    const auto conflicted = service->saveDraft();
    EXPECT_EQ(conflicted.error, std::make_optional(BenchmarkSaveError::Conflict));
    {
        QFile file(savedPath);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        EXPECT_EQ(file.readAll(), externallyEditedJson(benchmarkId.value));
    }

    // Refresh is suppressed while a dirty draft is open, so the pending rename is discarded
    // first; refresh then admits the external edit and the reopened definition is deletable.
    service->discardDraft();
    service->refresh();
    ASSERT_TRUE(service->openDraft(benchmarkId));
    ASSERT_TRUE(service->draft().has_value());
    EXPECT_EQ(service->draft()->name, "Externally Edited");

    const auto deleted = service->deleteBenchmark(benchmarkId);
    ASSERT_TRUE(deleted.ok());
    EXPECT_FALSE(QFile::exists(savedPath));
    ASSERT_TRUE(service->snapshot().has_value());
    EXPECT_TRUE(service->snapshot()->entries.empty());
    EXPECT_FALSE(service->hasDraft());
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
        std::shared_ptr<qt_data::BenchmarkRepository> repo;
        std::unique_ptr<App> app;
    };

    // A real App over a temp KovaaKs dir holding the "1wall6targets TE" perf and a temp benchmarks
    // dir holding `benchmarkJson`, run until the profile has loaded.
    std::unique_ptr<WiredWorkspace> wireWorkspace(QTemporaryDir &benchmarks, const QByteArray &benchmarkJson) {
        auto w = std::make_unique<WiredWorkspace>();
        if (!w->env.valid() || !w->env.makePerformancesDir()) return nullptr;
        if (w->env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty()) return nullptr;
        if (!benchmarks.isValid()) return nullptr;
        QFile file(QDir(benchmarks.path()).absoluteFilePath("bench.json"));
        if (!file.open(QIODevice::WriteOnly)) return nullptr;
        file.write(benchmarkJson);
        file.close();

        w->repo = std::make_shared<qt_data::BenchmarkRepository>(benchmarks.path().toStdString());
        w->app = std::make_unique<App>(w->env.settings, std::make_shared<data::ProtoDecoder>(),
                                       w->env.seriesConfigStore, nullptr, w->repo);
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
    EXPECT_NE(w->app->benchmarkLibraryService(), nullptr);
}

TEST(BenchmarkWorkspaceComposition, StartSuppliesBothBenchmarkViewModelsAsRequiredMainProperties) {
    integration::TestEnv env;
    ASSERT_TRUE(env.valid());

    App app(env.settings, std::make_shared<data::ProtoDecoder>(), env.seriesConfigStore);
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
    ASSERT_TRUE(manager->openDraft(domain::BenchmarkId{"b-cascade"}));
    ASSERT_TRUE(manager->setScenarioHash(domain::ScenarioEntryId{"s1"},
                                         std::optional<std::string>{kWallSixHash}).ok());
    ASSERT_TRUE(manager->saveDraft().ok());
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
