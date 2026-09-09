#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <ranges>

#include "benchmark_builders.h"
#include "counting_ids.h"
#include "fake_benchmark_repository.h"
#include "fake_playlist_reader.h"
#include "fake_profile_service.h"
#include "usecases/benchmark_library_service.h"

using namespace ksv;
using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    BenchmarkLibrarySnapshot savedWith(const std::vector<ScenarioEntry> &entries) {
        Benchmark benchmark;
        benchmark.id = BenchmarkId{"b1"};
        benchmark.name = "Saved";
        benchmark.uncategorized = entries;
        BenchmarkLibrarySnapshot snapshot;
        snapshot.entries.push_back({"b1.json", "d1", LoadedBenchmark{benchmark, validateBenchmark(benchmark)}});
        return snapshot;
    }

    struct Fixture {
        std::shared_ptr<FakeBenchmarkRepository> repo = std::make_shared<FakeBenchmarkRepository>();
        std::shared_ptr<FakePlaylistReader> reader = std::make_shared<FakePlaylistReader>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();

        void known(const std::string &name, const std::string &hash, int count = 1) {
            const ScenarioId scenario{name, hash};
            profile->scenarios.push_back(scenario);
            profile->run_counts[scenario] = count;
        }
        std::shared_ptr<BenchmarkLibraryService> build() {
            return std::make_shared<BenchmarkLibraryService>(repo, reader, profile, countingIds());
        }
    };

    const ScenarioResolution &forEntry(const std::vector<ScenarioResolution> &resolutions, const char *id) {
        return *std::ranges::find_if(resolutions, [id](const ScenarioResolution &resolution) {
            return resolution.entryId.value == id;
        });
    }

    BenchmarkWriteResult wrote(const std::string &digest) { return {digest, std::nullopt}; }
}

TEST(BenchmarkReconciliation, ClassifiesAllNameAndPersistedHashStates) {
    Fixture f;
    f.known("One", "h1", 3);
    f.profile->last_run_times[ScenarioId{"One", "h1"}] = std::chrono::sys_seconds{std::chrono::seconds{99}};
    f.known("Many", "hb", 7);
    f.known("Many", "ha", 2);
    f.repo->nextScan = {savedWith({benchmarkEntry("one", "One", std::nullopt, {}),
                                   benchmarkEntry("many", "Many", std::nullopt, {}),
                                   benchmarkEntry("none", "None", std::nullopt, {}),
                                   benchmarkEntry("live", "Live", std::string{"h1"}, {}),
                                   benchmarkEntry("gone", "Gone", std::string{"missing"}, {})}), std::nullopt};
    const auto service = f.build();
    const auto resolutions = service->resolutionsFor(BenchmarkId{"b1"});
    EXPECT_EQ(forEntry(resolutions, "one").state, ScenarioMatchState::AutoMappable);
    EXPECT_EQ(forEntry(resolutions, "many").state, ScenarioMatchState::Ambiguous);
    EXPECT_EQ(forEntry(resolutions, "none").state, ScenarioMatchState::Unresolved);
    EXPECT_EQ(forEntry(resolutions, "live").state, ScenarioMatchState::Resolved);
    EXPECT_EQ(forEntry(resolutions, "gone").state, ScenarioMatchState::MappedUnavailable);
    ASSERT_EQ(forEntry(resolutions, "one").candidates.size(), 1U);
    EXPECT_EQ(forEntry(resolutions, "one").candidates[0].runCount, 3);
    EXPECT_EQ(forEntry(resolutions, "one").candidates[0].lastPlayed,
              std::optional{std::chrono::sys_seconds{std::chrono::seconds{99}}});
    ASSERT_EQ(forEntry(resolutions, "many").candidates.size(), 2U);
    EXPECT_EQ(forEntry(resolutions, "many").candidates[0].hash, "ha");
}

TEST(BenchmarkReconciliation, ExposesSortedCatalogueAndDefinitionOrder) {
    Fixture f;
    f.known("Beta", "h2");
    f.known("Alpha", "hb");
    f.known("Alpha", "ha");
    Benchmark benchmark;
    benchmark.id = BenchmarkId{"b1"}; benchmark.name = "Saved";
    benchmark.uncategorized.push_back(benchmarkEntry("u", "U", std::nullopt, {}));
    Category category{GroupId{"c"}, "C", {}, {}, {}};
    category.scenarios.push_back(benchmarkEntry("d", "D", std::nullopt, {}));
    Subcategory sub{GroupId{"s"}, "S", {}, {benchmarkEntry("n", "N", std::nullopt, {})}};
    category.subcategories.push_back(sub); benchmark.categories.push_back(category);
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back({"b1.json", "d1", LoadedBenchmark{benchmark, validateBenchmark(benchmark)}});
    f.repo->nextScan = {snapshot, std::nullopt};
    const auto service = f.build();
    const auto catalogue = service->scenarioCatalogue();
    ASSERT_EQ(catalogue.size(), 3U);
    EXPECT_EQ(catalogue[0].hash, "ha"); EXPECT_EQ(catalogue[1].hash, "hb");
    const auto resolutions = service->resolutionsFor(BenchmarkId{"b1"});
    EXPECT_EQ(resolutions[0].entryId.value, "u"); EXPECT_EQ(resolutions[1].entryId.value, "d");
    EXPECT_EQ(resolutions[2].entryId.value, "n");
}

TEST(BenchmarkReconciliation, AutoMappingPersistsAtomicallyAndPublishesOnce) {
    Fixture f;
    f.known("Alpha", "h1");
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::nullopt, {})}), std::nullopt};
    f.repo->nextWrite = wrote("d2");
    const auto service = f.build();
    EXPECT_EQ(f.repo->lastWriteFilename, "b1.json");
    EXPECT_EQ(f.repo->lastWriteDigest, std::optional<std::string>{"d1"});
    EXPECT_EQ(service->revision(), 1U);
    EXPECT_EQ(forEntry(service->resolutionsFor(BenchmarkId{"b1"}), "e1").state, ScenarioMatchState::Resolved);
    EXPECT_EQ(service->snapshot()->entries[0].digest, "d2");
}

TEST(BenchmarkReconciliation, FailedAutomaticWriteDoesNotChangeAcceptedDefinition) {
    Fixture f;
    f.known("Alpha", "h1");
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::nullopt, {})}), std::nullopt};
    f.repo->nextWrite = {std::nullopt, BenchmarkWriteFailure::ExternalModificationConflict};
    const auto service = f.build();
    EXPECT_TRUE(service->lastResolutionWriteFailed());
    EXPECT_EQ(service->snapshot()->entries[0].digest, "d1");
    EXPECT_EQ(forEntry(service->resolutionsFor(BenchmarkId{"b1"}), "e1").state, ScenarioMatchState::AutoMappable);
}

TEST(BenchmarkReconciliation, ProfileChangeReconcilesAndSupportsMultipleSubscribers) {
    Fixture f;
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::nullopt, {})}), std::nullopt};
    f.repo->nextWrite = wrote("d2");
    const auto service = f.build();
    int secondSubscriber = 0;
    f.profile->onProfileChanged([&] { ++secondSubscriber; });
    f.known("Alpha", "h1");
    f.profile->notifyProfileChanged();
    EXPECT_EQ(secondSubscriber, 1);
    EXPECT_EQ(f.repo->lastWriteFilename, "b1.json");
    EXPECT_EQ(forEntry(service->resolutionsFor(BenchmarkId{"b1"}), "e1").state, ScenarioMatchState::Resolved);
}

TEST(BenchmarkReconciliation, AutoMappingNeverCreatesDuplicateHashes) {
    Fixture f;
    f.known("Alpha", "h1");
    f.repo->nextScan = {savedWith({benchmarkEntry("one", "Alpha", std::string{"h1"}, {}),
                                   benchmarkEntry("two", "Alpha", std::nullopt, {})}), std::nullopt};
    f.repo->nextWrite = wrote("d2");
    const auto service = f.build();
    EXPECT_TRUE(f.repo->lastWriteFilename.empty());
    EXPECT_EQ(forEntry(service->resolutionsFor(BenchmarkId{"b1"}), "two").state, ScenarioMatchState::AutoMappable);
}

// Resolution state is derived from the profile, so it can change with no file changing at all: a
// Mapped-unavailable entry becomes Resolved the moment its hash appears. Publishing only on a write
// would leave every consumer showing the stale match state.
TEST(BenchmarkReconciliation, ProfileChangePublishesEvenWhenNoFileChanges) {
    Fixture f;
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::string{"h1"}, {})}), std::nullopt};
    const auto service = f.build();
    ASSERT_EQ(forEntry(service->resolutionsFor(BenchmarkId{"b1"}), "e1").state,
              ScenarioMatchState::MappedUnavailable);

    int published = 0;
    service->onChanged([&] { ++published; });
    f.known("Alpha", "h1");
    f.profile->notifyProfileChanged();

    EXPECT_TRUE(f.repo->lastWriteFilename.empty());
    EXPECT_EQ(published, 1);
    EXPECT_EQ(forEntry(service->resolutionsFor(BenchmarkId{"b1"}), "e1").state, ScenarioMatchState::Resolved);
}

// The save write and the deferred mapping write share one publication. The tracking cache keys on
// the revision, so bumping it twice would throw away a projection for nothing.
TEST(BenchmarkReconciliation, SavingADraftPublishesOnce) {
    Fixture f;
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::nullopt, {})}), std::nullopt};
    const auto service = f.build();
    ASSERT_TRUE(service->openDraft(BenchmarkId{"b1"}));
    ASSERT_TRUE(service->renameBenchmark("Changed").ok());
    f.known("Alpha", "h1");
    f.profile->notifyProfileChanged();

    int published = 0;
    service->onChanged([&] { ++published; });
    f.repo->nextWrite = wrote("d2");
    ASSERT_TRUE(service->saveDraft().ok());

    EXPECT_EQ(published, 1);
}

// A problem entry carries no definition to reconcile, and skipping it must not be mistaken for an
// automatic write that failed.
TEST(BenchmarkReconciliation, ProblemEntriesAreSkipped) {
    Fixture f;
    f.known("Alpha", "h1");
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back({"broken.json", "dx",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}}});
    f.repo->nextScan = {snapshot, std::nullopt};
    f.repo->nextWrite = wrote("d2");

    const auto service = f.build();

    EXPECT_TRUE(f.repo->lastWriteFilename.empty());
    EXPECT_FALSE(service->lastResolutionWriteFailed());
}

TEST(BenchmarkReconciliation, DirtyDraftDefersThenDiscardPersistsMapping) {
    Fixture f;
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::nullopt, {})}), std::nullopt};
    f.repo->nextWrite = wrote("d2");
    const auto service = f.build();
    ASSERT_TRUE(service->openDraft(BenchmarkId{"b1"}));
    ASSERT_TRUE(service->renameBenchmark("Changed").ok());
    f.known("Alpha", "h1"); f.profile->notifyProfileChanged();
    EXPECT_TRUE(f.repo->lastWriteFilename.empty());
    EXPECT_EQ(service->draft()->name, "Changed");
    service->discardDraft();
    EXPECT_EQ(f.repo->lastWriteFilename, "b1.json");
}

TEST(BenchmarkReconciliation, CleanDraftAdoptsAutomaticWriteDigest) {
    Fixture f;
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::nullopt, {})}), std::nullopt};
    const auto service = f.build();
    ASSERT_TRUE(service->openDraft(BenchmarkId{"b1"}));
    f.repo->nextWrite = wrote("d2"); f.known("Alpha", "h1"); f.profile->notifyProfileChanged();
    ASSERT_TRUE(service->draft()->uncategorized[0].hash.has_value());
    ASSERT_TRUE(service->renameBenchmark("Changed").ok());
    f.repo->nextWrite = wrote("d3");
    EXPECT_TRUE(service->saveDraft().ok());
    EXPECT_EQ(f.repo->lastWriteDigest, std::optional<std::string>{"d2"});
}

TEST(BenchmarkReconciliation, SavingDirtyDraftReconcilesAndAdoptsDeferredMapping) {
    Fixture f;
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::nullopt, {})}), std::nullopt};
    const auto service = f.build();
    ASSERT_TRUE(service->openDraft(BenchmarkId{"b1"}));
    ASSERT_TRUE(service->renameBenchmark("Changed").ok());
    f.known("Alpha", "h1");
    f.profile->notifyProfileChanged();
    f.repo->nextWrite = wrote("d2");
    ASSERT_TRUE(service->saveDraft().ok());
    EXPECT_EQ(f.repo->lastWriteDigest, std::optional<std::string>{"d2"});
    ASSERT_TRUE(service->draft()->uncategorized[0].hash.has_value());
    EXPECT_EQ(*service->draft()->uncategorized[0].hash, "h1");
}

TEST(BenchmarkSetScenarioHash, EditsAndClearsOnlyTheDraft) {
    Fixture f;
    f.known("Alpha", "new");
    f.repo->nextScan = {savedWith({benchmarkEntry("e1", "Alpha", std::string{"old"}, {})}), std::nullopt};
    const auto service = f.build();
    ASSERT_TRUE(service->openDraft(BenchmarkId{"b1"}));
    EXPECT_TRUE(service->setScenarioHash(ScenarioEntryId{"e1"}, std::string{"chosen"}).ok());
    EXPECT_TRUE(service->draftDirty());
    EXPECT_EQ(*service->draftResolutions()[0].hash, "chosen");
    EXPECT_TRUE(service->setScenarioHash(ScenarioEntryId{"e1"}, std::nullopt).ok());
    EXPECT_EQ(service->draftResolutions()[0].state, ScenarioMatchState::AutoMappable);
    EXPECT_TRUE(service->setScenarioHash(ScenarioEntryId{"missing"}, std::string{"x"}).error.has_value());
}

TEST(BenchmarkSetScenarioHash, RejectsWithoutDraftAndLeavesDuplicateValidationToValidator) {
    Fixture f;
    f.repo->nextScan = {savedWith({benchmarkEntry("one", "One", std::string{"h"}, {}),
                                   benchmarkEntry("two", "Two", std::nullopt, {})}), std::nullopt};
    const auto service = f.build();
    EXPECT_EQ(service->setScenarioHash(ScenarioEntryId{"two"}, std::string{"h"}).error,
              std::optional{BenchmarkDraftError::NoDraft});
    ASSERT_TRUE(service->openDraft(BenchmarkId{"b1"}));
    ASSERT_TRUE(service->setScenarioHash(ScenarioEntryId{"two"}, std::string{"h"}).ok());
    EXPECT_TRUE(std::ranges::any_of(service->draftValidation().issues, [](const BenchmarkIssue &issue) {
        return issue.code == BenchmarkIssueCode::DuplicateResolvedHash;
    }));
}
