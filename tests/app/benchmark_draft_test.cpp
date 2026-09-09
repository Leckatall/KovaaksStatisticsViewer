#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "usecases/benchmark_library_service.h"
#include "counting_ids.h"
#include "fake_benchmark_repository.h"
#include "fake_playlist_reader.h"
#include "fake_profile_service.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    std::shared_ptr<BenchmarkLibraryService> makeService(std::shared_ptr<FakeBenchmarkRepository> repo,
                                                         std::shared_ptr<FakePlaylistReader> reader = nullptr) {
        return std::make_shared<BenchmarkLibraryService>(
            repo, reader ? std::move(reader) : std::make_shared<FakePlaylistReader>(),
            std::make_shared<FakeProfileService>(), countingIds());
    }

    Benchmark loadedBenchmark() {
        Benchmark loaded;
        loaded.id = BenchmarkId{"bench-A"};
        loaded.name = "Alpha";
        return loaded;
    }

    BenchmarkLibrarySnapshot snapshotWith(const Benchmark &loaded) {
        BenchmarkLibrarySnapshot snapshot;
        snapshot.entries.push_back(
            {"bench-A.json", "digest-A", LoadedBenchmark{loaded, validateBenchmark(loaded)}});
        return snapshot;
    }

    bool hasIssue(const CompletenessResult &result, BenchmarkIssueCode code) {
        for (const auto &issue: result.issues)
            if (issue.code == code) return true;
        return false;
    }

    const ScenarioEntry &entryAt(const Benchmark &benchmark, std::size_t index) {
        return benchmark.uncategorized.at(index);
    }
}

TEST(BenchmarkDraft, BeginNewDraftIsEmptyDirtyAndIncomplete) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    int notified = 0;
    service->onDraftChanged([&] { ++notified; });

    service->beginNewDraft();

    ASSERT_TRUE(service->hasDraft());
    EXPECT_TRUE(service->draftDirty());
    ASSERT_TRUE(service->draft().has_value());
    EXPECT_TRUE(service->draft()->tiers.empty());
    EXPECT_EQ(service->draftValidation().completeness, Completeness::Incomplete);
    EXPECT_EQ(notified, 1);
}

TEST(BenchmarkDraft, OpenDraftLoadsLoadedEntryByIdAndIsNotDirty) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshotWith(loadedBenchmark()), std::nullopt};
    auto service = makeService(repo);

    EXPECT_TRUE(service->openDraft(BenchmarkId{"bench-A"}));
    ASSERT_TRUE(service->draft().has_value());
    EXPECT_EQ(service->draft()->name, "Alpha");
    EXPECT_FALSE(service->draftDirty());
    EXPECT_TRUE(service->draftFromLibrary());
}

TEST(BenchmarkDraft, OpenDraftRejectsUnknownAndProblemEntries) {
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back({"bad.json", "d",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, BenchmarkId{"bench-P"}, {}}});
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshot, std::nullopt};
    auto service = makeService(repo);

    EXPECT_FALSE(service->openDraft(BenchmarkId{"missing"}));
    EXPECT_FALSE(service->openDraft(BenchmarkId{"bench-P"}));
    EXPECT_FALSE(service->hasDraft());
}

TEST(BenchmarkDraft, DiscardClearsTheDraft) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();

    service->discardDraft();

    EXPECT_FALSE(service->hasDraft());
    EXPECT_FALSE(service->draft().has_value());
}

TEST(BenchmarkDraft, RefreshIsExcludedWhileDraftIsDirty) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    const auto scansAfterCtor = repo->scanCount;

    service->beginNewDraft();  // dirty
    service->refresh();
    EXPECT_EQ(repo->scanCount, scansAfterCtor);  // refresh did not scan

    service->discardDraft();
    service->refresh();
    EXPECT_EQ(repo->scanCount, scansAfterCtor + 1);
}

TEST(BenchmarkDraft, AddTierAppendsWithFreshIdAndMarksDirty) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();

    const auto result = service->addTier("Gold");

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.createdTier.has_value());
    ASSERT_TRUE(service->draft().has_value());
    ASSERT_EQ(service->draft()->tiers.size(), 1U);
    EXPECT_EQ(service->draft()->tiers.front().name, "Gold");
    EXPECT_EQ(service->draft()->tiers.front().id, *result.createdTier);
    EXPECT_TRUE(service->draftDirty());
}

TEST(BenchmarkDraft, ReorderTierMovesTheTier) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    service->addTier("Bronze");
    service->addTier("Silver");
    service->addTier("Gold");
    const auto goldId = service->draft()->tiers.at(2).id;

    const auto result = service->reorderTier(goldId, 0);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(service->draft()->tiers.size(), 3U);
    EXPECT_EQ(service->draft()->tiers.at(0).name, "Gold");
    EXPECT_EQ(service->draft()->tiers.at(2).name, "Silver");
}

TEST(BenchmarkDraft, ReorderTierOutOfRangeReturnsPositionOutOfRange) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto tier = *service->addTier("Gold").createdTier;

    EXPECT_EQ(service->reorderTier(tier, 1).error,
              std::make_optional(BenchmarkDraftError::PositionOutOfRange));
    EXPECT_EQ(service->reorderTier(TierId{"nope"}, 0).error,
              std::make_optional(BenchmarkDraftError::UnknownTier));
}

TEST(BenchmarkDraft, TierCommandOnUnknownTierReturnsUnknownTier) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();

    EXPECT_EQ(service->renameTier(TierId{"nope"}, "x").error,
              std::make_optional(BenchmarkDraftError::UnknownTier));
    EXPECT_EQ(service->removeTier(TierId{"nope"}).error,
              std::make_optional(BenchmarkDraftError::UnknownTier));
}

TEST(BenchmarkDraft, RemoveTierAlsoRemovesItsThresholdsFromEveryEntry) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto tier = *service->addTier("Gold").createdTier;
    const auto otherTier = *service->addTier("Silver").createdTier;
    const auto entry = *service->addUnplayedScenario("1w6ts").createdEntry;
    ASSERT_TRUE(service->setThreshold(entry, tier, 10.0).ok());
    ASSERT_TRUE(service->setThreshold(entry, otherTier, 5.0).ok());

    const auto result = service->removeTier(tier);

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(service->draft().has_value());
    ASSERT_EQ(service->draft()->uncategorized.size(), 1U);
    // Only the removed tier's threshold is dropped; the surviving tier's stays.
    const auto &remaining = service->draft()->uncategorized.front().thresholds;
    ASSERT_EQ(remaining.size(), 1U);
    EXPECT_EQ(remaining.front().tierId, otherTier);
    EXPECT_EQ(service->draft()->tiers.size(), 1U);
    EXPECT_EQ(service->draft()->tiers.front().id, otherTier);
}

TEST(BenchmarkDraft, MutationWithNoDraftReturnsNoDraft) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);

    EXPECT_EQ(service->addTier("x").error, std::make_optional(BenchmarkDraftError::NoDraft));
    EXPECT_EQ(service->renameBenchmark("x").error, std::make_optional(BenchmarkDraftError::NoDraft));
    EXPECT_EQ(service->saveDraft().error, std::make_optional(BenchmarkSaveError::NoDraft));
}

TEST(BenchmarkDraft, AddScenarioRejectsDuplicateDisplayName) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    ASSERT_TRUE(service->addUnplayedScenario("1w6ts").ok());

    EXPECT_EQ(service->addUnplayedScenario("1w6ts").error,
              std::make_optional(BenchmarkDraftError::DuplicateScenarioName));
    EXPECT_EQ(service->addKnownScenario("1w6ts", "hash").error,
              std::make_optional(BenchmarkDraftError::DuplicateScenarioName));
    ASSERT_TRUE(service->draft().has_value());
    EXPECT_EQ(service->draft()->uncategorized.size(), 1U);
}

TEST(BenchmarkDraft, AddKnownScenarioPersistsHash) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();

    const auto result = service->addKnownScenario("1w6ts", "C0FFE00D");

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(service->draft().has_value());
    ASSERT_EQ(service->draft()->uncategorized.size(), 1U);
    ASSERT_TRUE(service->draft()->uncategorized.front().hash.has_value());
    EXPECT_EQ(*service->draft()->uncategorized.front().hash, "C0FFE00D");
}

TEST(BenchmarkDraft, SetThresholdUpsertsForTier) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto tier = *service->addTier("Gold").createdTier;
    const auto entry = *service->addUnplayedScenario("1w6ts").createdEntry;

    ASSERT_TRUE(service->setThreshold(entry, tier, 10.0).ok());
    ASSERT_TRUE(service->setThreshold(entry, tier, 20.0).ok());

    ASSERT_TRUE(service->draft().has_value());
    ASSERT_EQ(service->draft()->uncategorized.front().thresholds.size(), 1U);
    EXPECT_EQ(service->draft()->uncategorized.front().thresholds.front().score, 20.0);
    EXPECT_EQ(service->draft()->uncategorized.front().thresholds.front().tierId, tier);
}

TEST(BenchmarkDraft, ClearThresholdRemovesOnlyThatTiersThreshold) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto gold = *service->addTier("Gold").createdTier;
    const auto silver = *service->addTier("Silver").createdTier;
    const auto entry = *service->addUnplayedScenario("1w6ts").createdEntry;
    ASSERT_TRUE(service->setThreshold(entry, gold, 10.0).ok());
    ASSERT_TRUE(service->setThreshold(entry, silver, 5.0).ok());

    ASSERT_TRUE(service->clearThreshold(entry, gold).ok());

    ASSERT_TRUE(service->draft().has_value());
    ASSERT_EQ(service->draft()->uncategorized.front().thresholds.size(), 1U);
    EXPECT_EQ(service->draft()->uncategorized.front().thresholds.front().tierId, silver);
}

TEST(BenchmarkDraft, SetThresholdOnUnknownEntryOrTierReturnsError) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto tier = *service->addTier("Gold").createdTier;
    const auto entry = *service->addUnplayedScenario("1w6ts").createdEntry;

    EXPECT_EQ(service->setThreshold(entry, TierId{"nope"}, 1.0).error,
              std::make_optional(BenchmarkDraftError::UnknownTier));
    EXPECT_EQ(service->setThreshold(ScenarioEntryId{"nope"}, tier, 1.0).error,
              std::make_optional(BenchmarkDraftError::UnknownEntry));
    EXPECT_EQ(service->clearThreshold(ScenarioEntryId{"nope"}, tier).error,
              std::make_optional(BenchmarkDraftError::UnknownEntry));
}

TEST(BenchmarkDraft, RenameScenarioUpdatesTheRetainedDisplayName) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto entry = *service->addUnplayedScenario("old name").createdEntry;

    ASSERT_TRUE(service->renameScenario(entry, "new name").ok());

    ASSERT_TRUE(service->draft().has_value());
    EXPECT_EQ(service->draft()->uncategorized.front().name, "new name");
}

TEST(BenchmarkDraft, AddSubcategoryToPopulatedCategoryMovesDirectScenariosIntoASubcategory) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    service->renameBenchmark("Bench");
    const auto category = *service->addCategory("Clicking").createdGroup;
    const auto first = *service->addUnplayedScenario("s1").createdEntry;
    const auto second = *service->addUnplayedScenario("s2").createdEntry;
    ASSERT_TRUE(service->moveScenario(first, category).ok());
    ASSERT_TRUE(service->moveScenario(second, category).ok());

    const auto result = service->addSubcategory(category, "Static");

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.createdGroup.has_value());
    const auto &draft = *service->draft();
    ASSERT_EQ(draft.categories.size(), 1U);
    EXPECT_TRUE(draft.categories.front().scenarios.empty());
    ASSERT_EQ(draft.categories.front().subcategories.size(), 2U);
    ASSERT_EQ(draft.categories.front().subcategories.front().scenarios.size(), 2U);
    EXPECT_EQ(draft.categories.front().subcategories.front().scenarios.front().name, "s1");
    EXPECT_EQ(draft.categories.front().subcategories.back().name, "Static");
    EXPECT_TRUE(draft.categories.front().subcategories.back().scenarios.empty());
    EXPECT_FALSE(hasIssue(service->draftValidation(), BenchmarkIssueCode::MixedCategoryContent));
}

TEST(BenchmarkDraft, RemoveCategoryReturnsScenariosToUncategorized) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto category = *service->addCategory("Clicking").createdGroup;
    const auto entry = *service->addUnplayedScenario("s1").createdEntry;
    ASSERT_TRUE(service->moveScenario(entry, category).ok());

    const auto result = service->removeCategory(category);

    ASSERT_TRUE(result.ok());
    const auto &draft = *service->draft();
    EXPECT_TRUE(draft.categories.empty());
    ASSERT_EQ(draft.uncategorized.size(), 1U);
    EXPECT_EQ(draft.uncategorized.front().name, "s1");
}

TEST(BenchmarkDraft, RemoveSubcategoryReturnsItsScenariosToUncategorized) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto category = *service->addCategory("Clicking").createdGroup;
    const auto subcategory = *service->addSubcategory(category, "Static").createdGroup;
    const auto entry = *service->addUnplayedScenario("s1").createdEntry;
    ASSERT_TRUE(service->moveScenario(entry, subcategory).ok());
    ASSERT_EQ(service->draft()->categories.front().subcategories.size(), 1U);

    const auto result = service->removeCategory(subcategory);

    ASSERT_TRUE(result.ok());
    const auto &draft = *service->draft();
    ASSERT_EQ(draft.categories.size(), 1U);
    EXPECT_TRUE(draft.categories.front().subcategories.empty());
    ASSERT_EQ(draft.uncategorized.size(), 1U);
    EXPECT_EQ(draft.uncategorized.front().name, "s1");
}

TEST(BenchmarkDraft, MoveScenarioIntoCategoryThatHasSubcategoriesReturnsMixedContent) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto category = *service->addCategory("Clicking").createdGroup;
    ASSERT_TRUE(service->addUnplayedScenario("s1").ok());
    ASSERT_TRUE(service->addSubcategory(category, "Static").ok());
    const auto entry = *service->addUnplayedScenario("s2").createdEntry;

    EXPECT_EQ(service->moveScenario(entry, category).error,
              std::make_optional(BenchmarkDraftError::MixedContent));

    // The rejected move left the entry where it was.
    ASSERT_TRUE(service->draft().has_value());
    EXPECT_EQ(service->draft()->uncategorized.size(), 2U);
    EXPECT_EQ(service->draft()->uncategorized.back().name, "s2");
}

TEST(BenchmarkDraft, MoveScenarioBackToUncategorized) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto category = *service->addCategory("Clicking").createdGroup;
    const auto entry = *service->addUnplayedScenario("s1").createdEntry;
    ASSERT_TRUE(service->moveScenario(entry, category).ok());
    ASSERT_TRUE(service->draft()->categories.front().scenarios.size() == 1U);

    const auto result = service->moveScenario(entry, std::monostate{});

    ASSERT_TRUE(result.ok());
    const auto &draft = *service->draft();
    EXPECT_TRUE(draft.categories.front().scenarios.empty());
    ASSERT_EQ(draft.uncategorized.size(), 1U);
    EXPECT_EQ(draft.uncategorized.front().id, entry);
}

TEST(BenchmarkDraft, MoveScenarioToUnknownGroupReturnsUnknownGroup) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto entry = *service->addUnplayedScenario("s1").createdEntry;

    EXPECT_EQ(service->moveScenario(entry, GroupId{"nope"}).error,
              std::make_optional(BenchmarkDraftError::UnknownGroup));
}

TEST(BenchmarkDraft, ImportPlaylistCreatesDraftWithUniqueScenariosInSourceOrderUnderUncategorized) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto reader = std::make_shared<FakePlaylistReader>();
    reader->nextResult = {PlaylistSeed{std::string{"Voltaic"}, {"a", "b", "c"}, {2}}, std::nullopt};
    auto service = makeService(repo, reader);

    const auto result = service->importPlaylist("playlist.json");

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.skippedDuplicateIndices, (std::vector<int>{2}));
    EXPECT_EQ(reader->lastPath, "playlist.json");
    ASSERT_TRUE(service->hasDraft());
    EXPECT_TRUE(service->draftDirty());
    ASSERT_TRUE(service->draft().has_value());
    EXPECT_EQ(service->draft()->name, "Voltaic");
    ASSERT_EQ(service->draft()->uncategorized.size(), 3U);
    EXPECT_EQ(entryAt(*service->draft(), 0).name, "a");
    EXPECT_EQ(entryAt(*service->draft(), 2).name, "c");
    EXPECT_TRUE(service->draft()->tiers.empty());
}

TEST(BenchmarkDraft, ImportFailurePreservesExistingDraftAndLibrary) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto reader = std::make_shared<FakePlaylistReader>();
    reader->nextResult = {std::nullopt, PlaylistImportFailure::MalformedJson};
    auto service = makeService(repo, reader);
    service->beginNewDraft();
    const auto tier = *service->addTier("Gold").createdTier;

    const auto result = service->importPlaylist("playlist.json");

    EXPECT_EQ(result.failure, std::make_optional(PlaylistImportFailure::MalformedJson));
    ASSERT_TRUE(service->hasDraft());
    ASSERT_TRUE(service->draft().has_value());
    ASSERT_EQ(service->draft()->tiers.size(), 1U);
    EXPECT_EQ(service->draft()->tiers.front().id, tier);
}

TEST(BenchmarkDraft, ImportWithoutUsableScenarioListFailsAndKeepsDraft) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto reader = std::make_shared<FakePlaylistReader>();
    reader->nextResult = {std::nullopt, PlaylistImportFailure::NoUsableScenarioList};
    auto service = makeService(repo, reader);
    service->beginNewDraft();

    EXPECT_EQ(service->importPlaylist("playlist.json").failure,
              std::make_optional(PlaylistImportFailure::NoUsableScenarioList));
    EXPECT_TRUE(service->hasDraft());
}

TEST(BenchmarkDraft, SaveNewDraftWritesUnderIdFilenameAndPublishes) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    const auto benchmarkId = service->draft()->id;
    service->renameBenchmark("Alpha");
    ASSERT_TRUE(service->addTier("Gold").ok());
    repo->nextWrite = {std::string{"digest-1"}, std::nullopt};

    const auto result = service->saveDraft();

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(repo->lastWriteFilename, benchmarkId.value + ".json");
    EXPECT_FALSE(repo->lastWriteDigest.has_value());
    EXPECT_EQ(repo->lastWritten.name, "Alpha");
    EXPECT_EQ(service->revision(), 2U);
    ASSERT_TRUE(service->snapshot().has_value());
    ASSERT_EQ(service->snapshot()->entries.size(), 1U);
    EXPECT_EQ(service->snapshot()->entries.front().filename, benchmarkId.value + ".json");
    EXPECT_EQ(service->snapshot()->entries.front().digest, "digest-1");
    EXPECT_FALSE(service->draftDirty());
}

TEST(BenchmarkDraft, SaveRejectsEmptyName) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();

    EXPECT_EQ(service->saveDraft().error, std::make_optional(BenchmarkSaveError::EmptyName));
    service->renameBenchmark("   ");
    EXPECT_EQ(service->saveDraft().error, std::make_optional(BenchmarkSaveError::EmptyName));
    EXPECT_TRUE(repo->lastWriteFilename.empty());  // nothing was written
}

TEST(BenchmarkDraft, SaveConflictLeavesDraftAndSnapshotUnchanged) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    service->renameBenchmark("Alpha");
    repo->nextWrite = {std::nullopt, BenchmarkWriteFailure::ExternalModificationConflict};

    const auto result = service->saveDraft();

    EXPECT_EQ(result.error, std::make_optional(BenchmarkSaveError::Conflict));
    EXPECT_TRUE(service->draftDirty());
    EXPECT_EQ(service->revision(), 1U);  // unchanged
    ASSERT_TRUE(service->snapshot().has_value());
    EXPECT_TRUE(service->snapshot()->entries.empty());
}

TEST(BenchmarkDraft, SaveWriteFailedMapsToWriteFailed) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    auto service = makeService(repo);
    service->beginNewDraft();
    service->renameBenchmark("Alpha");
    repo->nextWrite = {std::nullopt, BenchmarkWriteFailure::WriteFailed};

    EXPECT_EQ(service->saveDraft().error, std::make_optional(BenchmarkSaveError::WriteFailed));
    EXPECT_TRUE(service->draftDirty());
}

TEST(BenchmarkDraft, ReopenedDraftSavesUnderItsOriginalFilenameWithExpectedDigest) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshotWith(loadedBenchmark()), std::nullopt};
    auto service = makeService(repo);
    ASSERT_TRUE(service->openDraft(BenchmarkId{"bench-A"}));
    ASSERT_TRUE(service->renameBenchmark("Beta").ok());
    repo->nextWrite = {std::string{"digest-2"}, std::nullopt};

    const auto result = service->saveDraft();

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(repo->lastWriteFilename, "bench-A.json");
    ASSERT_TRUE(repo->lastWriteDigest.has_value());
    EXPECT_EQ(*repo->lastWriteDigest, "digest-A");
    ASSERT_TRUE(service->snapshot().has_value());
    ASSERT_EQ(service->snapshot()->entries.size(), 1U);
    EXPECT_EQ(service->snapshot()->entries.front().filename, "bench-A.json");
    EXPECT_EQ(service->snapshot()->entries.front().digest, "digest-2");
    EXPECT_FALSE(service->draftDirty());
}

TEST(BenchmarkDraft, DeleteRemovesEntryAndBumpsRevisionAndDiscardsItsDraft) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshotWith(loadedBenchmark()), std::nullopt};
    auto service = makeService(repo);
    ASSERT_TRUE(service->openDraft(BenchmarkId{"bench-A"}));
    repo->nextRemove = {true, std::nullopt};

    const auto result = service->deleteBenchmark(BenchmarkId{"bench-A"});

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(repo->lastRemoveFilename, "bench-A.json");
    ASSERT_TRUE(service->snapshot().has_value());
    EXPECT_TRUE(service->snapshot()->entries.empty());
    EXPECT_EQ(service->revision(), 2U);
    EXPECT_FALSE(service->hasDraft());
}

TEST(BenchmarkDraft, DeleteConflictKeepsEntry) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshotWith(loadedBenchmark()), std::nullopt};
    auto service = makeService(repo);
    repo->nextRemove = {false, BenchmarkWriteFailure::ExternalModificationConflict};

    const auto result = service->deleteBenchmark(BenchmarkId{"bench-A"});

    EXPECT_EQ(result.error, std::make_optional(BenchmarkDeleteError::Conflict));
    ASSERT_TRUE(service->snapshot().has_value());
    EXPECT_EQ(service->snapshot()->entries.size(), 1U);
    EXPECT_EQ(service->revision(), 1U);
}

TEST(BenchmarkDraft, DeleteUnknownBenchmarkReturnsNotFound) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshotWith(loadedBenchmark()), std::nullopt};
    auto service = makeService(repo);

    EXPECT_EQ(service->deleteBenchmark(BenchmarkId{"missing"}).error,
              std::make_optional(BenchmarkDeleteError::NotFound));
}
