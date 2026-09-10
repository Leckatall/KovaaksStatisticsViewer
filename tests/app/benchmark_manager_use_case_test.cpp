#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "benchmark_builders.h"
#include "counting_ids.h"
#include "fake_benchmark_repository.h"
#include "fake_playlist_reader.h"
#include "fake_profile_service.h"
#include "contracts/benchmark_manager_state.h"
#include "contracts/i_benchmark_manager_use_case.h"
#include "usecases/benchmark_library_service.h"
#include "usecases/benchmark_manager_use_case.h"

using namespace ksv;
using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    struct Fixture {
        std::shared_ptr<FakeBenchmarkRepository> repo = std::make_shared<FakeBenchmarkRepository>();
        std::shared_ptr<FakePlaylistReader> reader = std::make_shared<FakePlaylistReader>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<BenchmarkLibraryService> service;
        std::unique_ptr<BenchmarkManagerUseCase> manager;

        void known(const std::string &name, const std::string &hash) {
            profile->scenarios.push_back({name, hash});
        }

        void build() {
            service = std::make_shared<BenchmarkLibraryService>(repo, reader, profile, countingIds());
            manager = std::make_unique<BenchmarkManagerUseCase>(service);
        }
    };

    Benchmark savedBenchmark(const std::string &id, const std::vector<ScenarioEntry> &entries) {
        Benchmark value;
        value.id = BenchmarkId{id};
        value.name = "Saved";
        value.tiers = {benchmarkTier("bronze"), benchmarkTier("silver"), benchmarkTier("gold")};
        value.uncategorized = entries;
        return value;
    }

    BenchmarkLibrarySnapshot snapshotOf(const Benchmark &value) {
        BenchmarkLibrarySnapshot snapshot;
        snapshot.entries.push_back(
            {value.id.value + ".json", "d-" + value.id.value,
             LoadedBenchmark{value, validateBenchmark(value)}});
        return snapshot;
    }

    std::vector<std::pair<std::string, double> > rungs() {
        return {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}};
    }
}

TEST(BenchmarkManagerUseCaseState, InitialStateIsIdleWithAcceptedLibraryAndNoDraft) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();

    const auto &state = fixture.manager->state();
    ASSERT_TRUE(state.library.has_value());
    EXPECT_TRUE(state.library->entries.empty());
    EXPECT_EQ(state.libraryRevision, fixture.service->revision());
    EXPECT_EQ(state.managedDirectoryPath, fixture.repo->directory);
    EXPECT_FALSE(state.draft.has_value());
    EXPECT_FALSE(state.draftFromLibrary);
    EXPECT_FALSE(state.draftDirty);
    EXPECT_TRUE(state.scenarioCatalogue.empty());
    EXPECT_TRUE(state.draftResolutions.empty());
    EXPECT_FALSE(state.refreshFailed);
    EXPECT_FALSE(state.resolutionWriteFailed);
}

TEST(BenchmarkManagerUseCaseState, LibrarySnapshotReplacementPropagatesAsOneCoherentValue) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    ASSERT_TRUE(fixture.manager->state().library->entries.empty());

    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs()),
                                         benchmarkEntry("e2", "Beta", std::string{"h2"}, rungs())})),
        std::nullopt};
    fixture.service->refresh();

    const auto &state = fixture.manager->state();
    ASSERT_TRUE(state.library.has_value());
    ASSERT_EQ(state.library->entries.size(), 1U);
    EXPECT_EQ(state.library->entries.front().filename, "b1.json");
    EXPECT_EQ(state.libraryRevision, fixture.service->revision());
    EXPECT_EQ(state.scenarioCatalogue, fixture.service->scenarioCatalogue());
}

TEST(BenchmarkManagerUseCaseState, BeginningANewDraftPublishesADirtyIncompleteDraft) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();

    fixture.service->beginNewDraft();

    const auto &state = fixture.manager->state();
    ASSERT_TRUE(state.draft.has_value());
    EXPECT_TRUE(state.draftDirty);
    EXPECT_FALSE(state.draftFromLibrary);
    EXPECT_EQ(state.draftCompleteness.completeness, Completeness::Incomplete);
    EXPECT_FALSE(state.draftCompleteness.issues.empty());
}

TEST(BenchmarkManagerUseCaseState, OpeningALibraryBenchmarkPublishesACleanFromLibraryDraft) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    const auto value = savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs()),
                                             benchmarkEntry("e2", "Beta", std::string{"h2"}, rungs())});
    fixture.repo->nextScan = {snapshotOf(value), std::nullopt};
    fixture.build();

    ASSERT_TRUE(fixture.service->openDraft(BenchmarkId{"b1"}));

    const auto &state = fixture.manager->state();
    ASSERT_TRUE(state.draft.has_value());
    EXPECT_EQ(state.draft->id.value, "b1");
    EXPECT_TRUE(state.draftFromLibrary);
    EXPECT_FALSE(state.draftDirty);
    EXPECT_EQ(state.draftCompleteness.completeness, validateBenchmark(value).completeness);
}

TEST(BenchmarkManagerUseCaseState, ProfileCatalogueAndDraftResolutionsAreProjectedIntoState) {
    Fixture fixture;
    fixture.known("Alpha", "ha");
    fixture.known("Alpha", "hb");
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, {})})),
        std::nullopt};
    fixture.build();
    ASSERT_TRUE(fixture.service->openDraft(BenchmarkId{"b1"}));

    const auto &state = fixture.manager->state();
    EXPECT_EQ(state.scenarioCatalogue, fixture.service->scenarioCatalogue());
    ASSERT_EQ(state.draftResolutions.size(), fixture.service->draftResolutions().size());
    ASSERT_EQ(state.draftResolutions.size(), 1U);
    const auto &resolution = state.draftResolutions.front();
    EXPECT_EQ(resolution.entryId.value, "e1");
    EXPECT_EQ(resolution.state, ScenarioMatchState::Ambiguous);
    ASSERT_EQ(resolution.candidates.size(), 2U);
    EXPECT_EQ(resolution.candidates[0].hash, "ha");
    EXPECT_EQ(resolution.candidates[1].hash, "hb");
}

TEST(BenchmarkManagerUseCaseState, WholeDirectoryRefreshFailureSurfacesAsADiagnosticAndKeepsAcceptedLibrary) {
    Fixture fixture;
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())})),
        std::nullopt};
    fixture.build();
    ASSERT_EQ(fixture.manager->state().library->entries.size(), 1U);

    fixture.repo->nextScan = {std::nullopt, BenchmarkScanFailure::DirectoryUnavailable};
    fixture.service->refresh();

    const auto &state = fixture.manager->state();
    EXPECT_TRUE(state.refreshFailed);
    ASSERT_TRUE(state.library.has_value());
    ASSERT_EQ(state.library->entries.size(), 1U);
    EXPECT_EQ(state.library->entries.front().filename, "b1.json");
}

// One profile scenario matches an unmapped entry by name, so reconciliation attempts to persist
// the automatic mapping; the scripted write failure must reach the manager state as a flag rather
// than a thrown error or a lost snapshot.
TEST(BenchmarkManagerUseCaseState, AutomaticResolutionWriteFailureSurfacesInState) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, rungs())})),
        std::nullopt};
    fixture.repo->nextWrite = {std::nullopt, BenchmarkWriteFailure::WriteFailed};
    fixture.build();

    EXPECT_TRUE(fixture.manager->state().resolutionWriteFailed);
}

TEST(BenchmarkManagerUseCaseState, OneNotificationPerCoherentReplacement) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();

    int notifications = 0;
    fixture.manager->onChanged([&] { ++notifications; });

    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())})),
        std::nullopt};
    fixture.service->refresh();
    EXPECT_EQ(notifications, 1);

    fixture.service->beginNewDraft();
    EXPECT_EQ(notifications, 2);
}

// saveDraft() fires both service publication families in one call (library publish + draft change).
// The manager must coalesce that into a single onChanged for its consumers.
TEST(BenchmarkManagerUseCaseState, SaveDoesNotDoubleFireAcrossPublicationFamilies) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    fixture.service->beginNewDraft();
    fixture.service->renameBenchmark("Kept");

    int notifications = 0;
    fixture.manager->onChanged([&] { ++notifications; });

    fixture.repo->nextWrite = {std::string{"digest-1"}, std::nullopt};
    ASSERT_TRUE(fixture.service->saveDraft().ok());

    EXPECT_EQ(notifications, 1);
}

// state() hands back a const reference with the same lifetime rule as BenchmarkWorkspaceSnapshot:
// valid only until the next mutation/callback, never retained. A fresh call after a mutation must
// observe the new values.
TEST(BenchmarkManagerUseCaseState, StateReferenceReflectsLatestValuesOnReread) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    EXPECT_FALSE(fixture.manager->state().draft.has_value());

    fixture.service->beginNewDraft();

    ASSERT_TRUE(fixture.manager->state().draft.has_value());
    EXPECT_TRUE(fixture.manager->state().draftDirty);
}

// ---- Task A4: every manager command routed through IBenchmarkManagerUseCase ----------------
//
// The use case is the sole application boundary the migrated BenchmarkManagerViewModel talks to,
// so every authoring/mapping command it invokes today on IBenchmarkLibraryService must exist on
// IBenchmarkManagerUseCase with the same signature and result type, forward the service's return
// value unchanged, refresh state() before returning, and — on a scripted service failure — leave
// the accepted library and the current draft untouched.

namespace {
    struct DraftIds {
        domain::TierId tier;
        domain::TierId otherTier;
        domain::ScenarioEntryId entry;
        domain::GroupId category;
    };

    struct DraftCommandCase {
        std::string label;
        std::function<BenchmarkDraftResult(BenchmarkManagerUseCase &, const DraftIds &)> invoke;
    };

    // Every draft-mutating command the manager dialog drives, each returning BenchmarkDraftResult.
    // "clearScenarioHash" is the optional-hash clear path: setScenarioHash(entryId, std::nullopt).
    std::vector<DraftCommandCase> draftCommandCases() {
        return {
            {"renameBenchmark",
             [](BenchmarkManagerUseCase &m, const DraftIds &) { return m.renameBenchmark("Renamed"); }},
            {"addTier",
             [](BenchmarkManagerUseCase &m, const DraftIds &) { return m.addTier("Platinum"); }},
            {"renameTier",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) { return m.renameTier(ids.tier, "Tin"); }},
            {"setTierColor",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) {
                 return m.setTierColor(ids.tier, domain::BenchmarkColor{1, 2, 3, 255});
             }},
            {"reorderTier",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) { return m.reorderTier(ids.otherTier, 0); }},
            {"removeTier",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) { return m.removeTier(ids.otherTier); }},
            {"addUnplayedScenario",
             [](BenchmarkManagerUseCase &m, const DraftIds &) { return m.addUnplayedScenario("Fresh"); }},
            {"addKnownScenario",
             [](BenchmarkManagerUseCase &m, const DraftIds &) { return m.addKnownScenario("Known", "hK"); }},
            {"renameScenario",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) {
                 return m.renameScenario(ids.entry, "Renamed Scenario");
             }},
            {"setScenarioHash",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) {
                 return m.setScenarioHash(ids.entry, std::optional<std::string>{"h9"});
             }},
            {"clearScenarioHash",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) {
                 return m.setScenarioHash(ids.entry, std::nullopt);
             }},
            {"removeScenario",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) { return m.removeScenario(ids.entry); }},
            {"setThreshold",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) {
                 return m.setThreshold(ids.entry, ids.tier, 42.0);
             }},
            {"clearThreshold",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) {
                 return m.clearThreshold(ids.entry, ids.tier);
             }},
            {"addCategory",
             [](BenchmarkManagerUseCase &m, const DraftIds &) { return m.addCategory("Tracking"); }},
            {"renameGroup",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) { return m.renameGroup(ids.category, "Flicking"); }},
            {"setGroupColor",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) {
                 return m.setGroupColor(ids.category, domain::BenchmarkColor{4, 5, 6, 255});
             }},
            {"reorderCategory",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) { return m.reorderCategory(ids.category, 0); }},
            {"addSubcategory",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) { return m.addSubcategory(ids.category, "Sub"); }},
            {"moveScenario",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) {
                 return m.moveScenario(ids.entry, DraftGroupTarget{ids.category});
             }},
            {"removeCategory",
             [](BenchmarkManagerUseCase &m, const DraftIds &ids) { return m.removeCategory(ids.category); }},
        };
    }
}

TEST(BenchmarkManagerUseCaseCommands, DraftMutationsForwardOkAndRefreshStateFromTheService) {
    for (const auto &command: draftCommandCases()) {
        Fixture fixture;
        fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
        fixture.build();
        fixture.service->beginNewDraft();
        fixture.service->renameBenchmark("Base");
        DraftIds ids;
        ids.tier = *fixture.service->addTier("Bronze").createdTier;
        ids.otherTier = *fixture.service->addTier("Silver").createdTier;
        ids.entry = *fixture.service->addUnplayedScenario("Scenario One").createdEntry;
        ASSERT_TRUE(fixture.service->setThreshold(ids.entry, ids.tier, 10.0).ok()) << command.label;
        ids.category = *fixture.service->addCategory("Category One").createdGroup;

        const auto result = command.invoke(*fixture.manager, ids);

        EXPECT_TRUE(result.ok()) << command.label;
        EXPECT_EQ(fixture.manager->state().draft, fixture.service->draft()) << command.label;
        EXPECT_EQ(fixture.manager->state().draftDirty, fixture.service->draftDirty()) << command.label;
        EXPECT_EQ(fixture.manager->state().draftCompleteness.completeness,
                  fixture.service->draftValidation().completeness)
            << command.label;
        EXPECT_EQ(fixture.manager->state().libraryRevision, fixture.service->revision()) << command.label;
    }
}

TEST(BenchmarkManagerUseCaseCommands, DraftMutationErrorsForwardVerbatimAndPreserveLibraryAndDraft) {
    Fixture fixture;
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())})),
        std::nullopt};
    fixture.build();
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    ASSERT_FALSE(fixture.manager->state().draft.has_value());
    const auto revisionBefore = fixture.manager->state().libraryRevision;
    const auto entriesBefore = fixture.manager->state().library->entries.size();

    const DraftIds dummy{domain::TierId{"t"}, domain::TierId{"t2"}, domain::ScenarioEntryId{"e"},
                         domain::GroupId{"g"}};

    for (const auto &command: draftCommandCases()) {
        const auto result = command.invoke(*fixture.manager, dummy);
        ASSERT_TRUE(result.error.has_value()) << command.label;
        EXPECT_EQ(*result.error, BenchmarkDraftError::NoDraft) << command.label;
        EXPECT_FALSE(fixture.manager->state().draft.has_value()) << command.label;
        EXPECT_EQ(fixture.manager->state().libraryRevision, revisionBefore) << command.label;
        ASSERT_TRUE(fixture.manager->state().library.has_value()) << command.label;
        EXPECT_EQ(fixture.manager->state().library->entries.size(), entriesBefore) << command.label;
    }
}

// Literal "forwarded unchanged": the same command run through the service directly and through
// the manager yields the same ok()/error and the same shape of created-id.
TEST(BenchmarkManagerUseCaseCommands, DraftResultIsForwardedFromTheServiceVerbatim) {
    Fixture viaService;
    viaService.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    viaService.build();
    viaService.service->beginNewDraft();

    Fixture viaManager;
    viaManager.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    viaManager.build();
    viaManager.manager->beginNewDraft();

    const auto expected = viaService.service->addTier("Gold");
    const auto actual = viaManager.manager->addTier("Gold");

    EXPECT_EQ(actual.ok(), expected.ok());
    EXPECT_EQ(actual.error, expected.error);
    EXPECT_EQ(actual.createdTier.has_value(), expected.createdTier.has_value());
}

TEST(BenchmarkManagerUseCaseCommands, BeginNewDraftPublishesADirtyIncompleteDraft) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    int notifications = 0;
    fixture.manager->onChanged([&] { ++notifications; });

    fixture.manager->beginNewDraft();

    ASSERT_TRUE(fixture.manager->state().draft.has_value());
    EXPECT_TRUE(fixture.manager->state().draftDirty);
    EXPECT_FALSE(fixture.manager->state().draftFromLibrary);
    EXPECT_EQ(fixture.manager->state().draftCompleteness.completeness, Completeness::Incomplete);
    EXPECT_EQ(notifications, 1);
}

TEST(BenchmarkManagerUseCaseCommands, OpenLibraryBenchmarkForwardsTrueAndPublishesAFromLibraryDraft) {
    Fixture fixture;
    const auto value = savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())});
    fixture.repo->nextScan = {snapshotOf(value), std::nullopt};
    fixture.build();

    EXPECT_TRUE(fixture.manager->openDraft(BenchmarkId{"b1"}));

    ASSERT_TRUE(fixture.manager->state().draft.has_value());
    EXPECT_EQ(fixture.manager->state().draft->id.value, "b1");
    EXPECT_TRUE(fixture.manager->state().draftFromLibrary);
    EXPECT_FALSE(fixture.manager->state().draftDirty);
}

TEST(BenchmarkManagerUseCaseCommands, OpenUnknownBenchmarkForwardsFalseAndLeavesStateWithoutADraft) {
    Fixture fixture;
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())})),
        std::nullopt};
    fixture.build();

    EXPECT_FALSE(fixture.manager->openDraft(BenchmarkId{"missing"}));
    EXPECT_FALSE(fixture.manager->state().draft.has_value());
}

TEST(BenchmarkManagerUseCaseCommands, DiscardDraftClearsTheDraftFromState) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    fixture.manager->beginNewDraft();
    ASSERT_TRUE(fixture.manager->state().draft.has_value());

    fixture.manager->discardDraft();

    EXPECT_FALSE(fixture.manager->state().draft.has_value());
}

TEST(BenchmarkManagerUseCaseCommands, RefreshRescansAndRepublishesTheAcceptedLibrary) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    const auto scansBefore = fixture.repo->scanCount;

    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())})),
        std::nullopt};
    fixture.manager->refresh();

    EXPECT_EQ(fixture.repo->scanCount, scansBefore + 1);
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    ASSERT_EQ(fixture.manager->state().library->entries.size(), 1U);
    EXPECT_FALSE(fixture.manager->state().refreshFailed);
    EXPECT_EQ(fixture.manager->state().libraryRevision, fixture.service->revision());
}

TEST(BenchmarkManagerUseCaseCommands, RefreshFailureForwardsAndPreservesTheAcceptedLibrary) {
    Fixture fixture;
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())})),
        std::nullopt};
    fixture.build();

    fixture.repo->nextScan = {std::nullopt, BenchmarkScanFailure::DirectoryUnavailable};
    fixture.manager->refresh();

    EXPECT_TRUE(fixture.manager->state().refreshFailed);
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    ASSERT_EQ(fixture.manager->state().library->entries.size(), 1U);
    EXPECT_EQ(fixture.manager->state().library->entries.front().filename, "b1.json");
}

TEST(BenchmarkManagerUseCaseCommands, ImportPlaylistForwardsResultAndPublishesADraft) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.reader->nextResult = {PlaylistSeed{std::string{"Voltaic"}, {"a", "b"}, {}}, std::nullopt};
    fixture.build();

    const auto result = fixture.manager->importPlaylist("playlist.json");

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(fixture.reader->lastPath, "playlist.json");
    ASSERT_TRUE(fixture.manager->state().draft.has_value());
    EXPECT_EQ(fixture.manager->state().draft->name, "Voltaic");
    EXPECT_TRUE(fixture.manager->state().draftDirty);
}

TEST(BenchmarkManagerUseCaseCommands, ImportFailureForwardsUnchangedAndPreservesDraftAndLibrary) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.reader->nextResult = {std::nullopt, PlaylistImportFailure::MalformedJson};
    fixture.build();
    fixture.manager->beginNewDraft();
    fixture.manager->renameBenchmark("Kept");
    const auto draftBefore = fixture.manager->state().draft;
    const auto revisionBefore = fixture.manager->state().libraryRevision;

    const auto result = fixture.manager->importPlaylist("playlist.json");

    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(*result.failure, PlaylistImportFailure::MalformedJson);
    EXPECT_EQ(fixture.manager->state().draft, draftBefore);
    EXPECT_EQ(fixture.manager->state().libraryRevision, revisionBefore);
}

TEST(BenchmarkManagerUseCaseCommands, SaveForwardsResultAndPublishesTheNewLibraryEntry) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    fixture.manager->beginNewDraft();
    fixture.manager->renameBenchmark("Alpha");
    ASSERT_TRUE(fixture.manager->addTier("Gold").ok());
    fixture.repo->nextWrite = {std::string{"digest-1"}, std::nullopt};

    const auto result = fixture.manager->saveDraft();

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    ASSERT_EQ(fixture.manager->state().library->entries.size(), 1U);
    EXPECT_EQ(fixture.manager->state().libraryRevision, fixture.service->revision());
    EXPECT_FALSE(fixture.manager->state().draftDirty);
}

TEST(BenchmarkManagerUseCaseCommands, SaveWriteFailureForwardsUnchangedAndPreservesLibraryAndDraft) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    fixture.manager->beginNewDraft();
    fixture.manager->renameBenchmark("Alpha");
    const auto draftBefore = fixture.manager->state().draft;
    const auto revisionBefore = fixture.manager->state().libraryRevision;
    fixture.repo->nextWrite = {std::nullopt, BenchmarkWriteFailure::WriteFailed};

    const auto result = fixture.manager->saveDraft();

    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(*result.error, BenchmarkSaveError::WriteFailed);
    EXPECT_EQ(fixture.manager->state().draft, draftBefore);
    EXPECT_EQ(fixture.manager->state().libraryRevision, revisionBefore);
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    EXPECT_TRUE(fixture.manager->state().library->entries.empty());
}

TEST(BenchmarkManagerUseCaseCommands, DeleteForwardsOutcomeAndRepublishesLibraryWithoutTheEntry) {
    Fixture fixture;
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())})),
        std::nullopt};
    fixture.build();
    fixture.repo->nextRemove = {true, std::nullopt};

    const auto result = fixture.manager->deleteBenchmark(BenchmarkId{"b1"});

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    EXPECT_TRUE(fixture.manager->state().library->entries.empty());
    EXPECT_EQ(fixture.manager->state().libraryRevision, fixture.service->revision());
}

TEST(BenchmarkManagerUseCaseCommands, DeleteConflictForwardsUnchangedAndPreservesLibrary) {
    Fixture fixture;
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs())})),
        std::nullopt};
    fixture.build();
    const auto revisionBefore = fixture.manager->state().libraryRevision;
    fixture.repo->nextRemove = {false, BenchmarkWriteFailure::ExternalModificationConflict};

    const auto result = fixture.manager->deleteBenchmark(BenchmarkId{"b1"});

    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(*result.error, BenchmarkDeleteError::Conflict);
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    ASSERT_EQ(fixture.manager->state().library->entries.size(), 1U);
    EXPECT_EQ(fixture.manager->state().libraryRevision, revisionBefore);
}

TEST(BenchmarkManagerUseCaseCommands, ManagedDirectoryPathQueryForwardsFromTheService) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.repo->directory = "/custom/benchmarks";
    fixture.build();

    EXPECT_EQ(fixture.manager->managedDirectoryPath(), fixture.repo->managedDirectoryPath());
}

// The optional-hash clear path: setScenarioHash(entryId, std::nullopt) drops the mapping and
// returns the entry to reconciliation, keeping the entry and its thresholds in the draft.
TEST(BenchmarkManagerUseCaseCommands, ClearScenarioHashReturnsTheEntryToReconciliation) {
    Fixture fixture;
    fixture.known("Alpha", "ha");
    fixture.known("Alpha", "hb");
    fixture.repo->nextScan = {
        snapshotOf(savedBenchmark("b1", {benchmarkEntry("e1", "Alpha", std::string{"ha"}, rungs())})),
        std::nullopt};
    fixture.build();
    ASSERT_TRUE(fixture.service->openDraft(BenchmarkId{"b1"}));

    const auto result = fixture.manager->setScenarioHash(ScenarioEntryId{"e1"}, std::nullopt);

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(fixture.manager->state().draft.has_value());
    ASSERT_EQ(fixture.manager->state().draft->uncategorized.size(), 1U);
    EXPECT_FALSE(fixture.manager->state().draft->uncategorized.front().hash.has_value());
    EXPECT_FALSE(fixture.manager->state().draft->uncategorized.front().thresholds.empty());
    EXPECT_EQ(fixture.manager->state().draft, fixture.service->draft());
}
