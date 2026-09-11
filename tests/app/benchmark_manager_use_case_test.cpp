#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "benchmark_builders.h"
#include "counting_ids.h"
#include "fake_benchmark_resolution_use_case.h"
#include "fake_benchmarks_service.h"
#include "fake_playlist_reader.h"
#include "contracts/benchmark_editor_seed.h"
#include "contracts/benchmark_manager_state.h"
#include "usecases/benchmark_manager_use_case.h"

using namespace ksv;
using namespace ksv::application;
using namespace ksv::data;
using namespace ksv::domain;
using namespace ksv::tests_support;

// Task P2 seam coverage: the whole-benchmark manager contract composes accepted-library and
// resolution state, issues editor seeds behind a single edit lease, and forwards whole-value
// save/delete/resolve calls without retaining a working copy. Task P3 adds deeper behavioural RED.

namespace {
    struct Fixture {
        std::shared_ptr<FakeBenchmarksService> benchmarks = std::make_shared<FakeBenchmarksService>();
        std::shared_ptr<FakeBenchmarkResolutionUseCase> resolution =
            std::make_shared<FakeBenchmarkResolutionUseCase>();
        std::shared_ptr<FakePlaylistReader> reader = std::make_shared<FakePlaylistReader>();
        std::unique_ptr<BenchmarkManagerUseCase> manager;

        void build() {
            manager = std::make_unique<BenchmarkManagerUseCase>(benchmarks, resolution, reader,
                                                                countingIds());
        }
    };

    Benchmark savedBenchmark(const std::string &id) {
        Benchmark value;
        value.id = BenchmarkId{id};
        value.name = "Saved";
        value.tiers = {benchmarkTier("bronze"), benchmarkTier("silver"), benchmarkTier("gold")};
        value.uncategorized = {benchmarkEntry("e1", "Alpha", std::string{"h1"},
                                              {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}})};
        return value;
    }

    BenchmarkLibrarySnapshot snapshotOf(const Benchmark &value) {
        BenchmarkLibrarySnapshot snapshot;
        snapshot.entries.push_back({value.id.value + ".json", "d-" + value.id.value,
                                    LoadedBenchmark{value, validateBenchmark(value)}});
        return snapshot;
    }
}

TEST(BenchmarkManagerUseCaseState, ComposesAcceptedAndResolutionStateWithoutAnyDraftFields) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.benchmarks->revisionValue = 7;
    fixture.benchmarks->refreshFailed = false;
    fixture.benchmarks->directory = "/managed/benchmarks";
    fixture.resolution->snapshotValue.scenarioCatalogue = {ScenarioId{"Alpha", "h1"}};
    fixture.resolution->snapshotValue.resolutions[BenchmarkId{"b1"}] = {resolvedEntry("e1", "h1")};
    fixture.resolution->snapshotValue.automaticWriteFailures[BenchmarkId{"b1"}] =
        AutomaticMappingWriteError::WriteFailed;
    fixture.build();

    const auto &state = fixture.manager->state();
    ASSERT_TRUE(state.library.has_value());
    ASSERT_EQ(state.library->entries.size(), 1U);
    EXPECT_EQ(state.libraryRevision, 7U);
    EXPECT_FALSE(state.refreshFailed);
    EXPECT_EQ(state.managedDirectoryPath, "/managed/benchmarks");
    EXPECT_EQ(state.scenarioCatalogue.size(), 1U);
    ASSERT_EQ(state.resolutions.count(BenchmarkId{"b1"}), 1U);
    EXPECT_EQ(state.resolutions.at(BenchmarkId{"b1"}).front().state, ScenarioMatchState::Resolved);
    EXPECT_EQ(state.automaticWriteFailures.count(BenchmarkId{"b1"}), 1U);
    EXPECT_TRUE(state.resolutionWriteFailed);
}

TEST(BenchmarkManagerUseCaseState, EitherServiceOrResolutionPublicationRebuildsStateOnce) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = BenchmarkLibrarySnapshot{};
    fixture.build();

    int notifications = 0;
    fixture.manager->onChanged([&] { ++notifications; });

    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.benchmarks->revisionValue = 1;
    fixture.benchmarks->fireChanged();
    EXPECT_EQ(notifications, 1);

    fixture.resolution->snapshotValue.scenarioCatalogue = {ScenarioId{"Alpha", "h1"}};
    fixture.resolution->fireChanged();
    EXPECT_EQ(notifications, 2);

    // A publication that changes nothing a consumer observes is coalesced away.
    fixture.resolution->fireChanged();
    EXPECT_EQ(notifications, 2);
}

TEST(BenchmarkManagerUseCase, BeginNewBenchmarkReturnsATokenlessSeedAndLeasesItsReservedId) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = BenchmarkLibrarySnapshot{};
    fixture.build();

    const auto seed = fixture.manager->beginNewBenchmark();

    EXPECT_FALSE(seed.token.has_value());
    EXPECT_FALSE(seed.benchmark.id.value.empty());
    ASSERT_TRUE(fixture.resolution->editLease.has_value());
    EXPECT_EQ(*fixture.resolution->editLease, seed.benchmark.id);
}

TEST(BenchmarkManagerUseCase, OpenAcceptedReturnsCopyPlusTokenAndLeasesItsId) {
    Fixture fixture;
    const auto value = savedBenchmark("b1");
    fixture.benchmarks->snapshotValue = snapshotOf(value);
    fixture.build();

    const auto seed = fixture.manager->openBenchmark(BenchmarkId{"b1"});

    ASSERT_TRUE(seed.has_value());
    EXPECT_EQ(seed->benchmark.id.value, "b1");
    EXPECT_EQ(seed->benchmark.name, "Saved");
    ASSERT_TRUE(seed->token.has_value());
    EXPECT_EQ(seed->token->filename, "b1.json");
    EXPECT_EQ(seed->token->digest, "d-b1");
    ASSERT_TRUE(fixture.resolution->editLease.has_value());
    EXPECT_EQ(fixture.resolution->editLease->value, "b1");
}

TEST(BenchmarkManagerUseCase, OpenUnknownReturnsNulloptAndChangesNoLease) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.build();

    EXPECT_FALSE(fixture.manager->openBenchmark(BenchmarkId{"missing"}).has_value());
    EXPECT_EQ(fixture.resolution->setEditLeaseCount, 0);
}

TEST(BenchmarkManagerUseCase, OpenProblemEntryReturnsNulloptAndChangesNoLease) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back({"broken.json", "d",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, std::nullopt,
                                                 BenchmarkId{"b1"}, std::nullopt}});
    fixture.benchmarks->snapshotValue = snapshot;
    fixture.build();

    EXPECT_FALSE(fixture.manager->openBenchmark(BenchmarkId{"b1"}).has_value());
    EXPECT_EQ(fixture.resolution->setEditLeaseCount, 0);
}

TEST(BenchmarkManagerUseCase, CloseEditorReleasesTheLease) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.build();
    fixture.manager->openBenchmark(BenchmarkId{"b1"});

    fixture.manager->closeEditor();

    EXPECT_FALSE(fixture.resolution->editLease.has_value());
}

TEST(BenchmarkManagerUseCase, SaveForwardsTheExactValueAndTokenAndReturnsTheOutcomeUnchanged) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.build();
    const auto value = savedBenchmark("b1");
    const BenchmarkEditToken token{BenchmarkId{"b1"}, "b1.json", "d-b1"};
    fixture.benchmarks->nextSaveOutcome = {BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d-b1-new"},
                                           std::nullopt};

    const auto outcome = fixture.manager->save(value, token);

    ASSERT_EQ(fixture.benchmarks->saveRequests.size(), 1U);
    EXPECT_EQ(fixture.benchmarks->saveRequests.front().benchmark.id.value, "b1");
    ASSERT_TRUE(fixture.benchmarks->saveRequests.front().token.has_value());
    EXPECT_EQ(fixture.benchmarks->saveRequests.front().token->digest, "d-b1");
    ASSERT_TRUE(outcome.ok());
    EXPECT_EQ(outcome.token->digest, "d-b1-new");
}

TEST(BenchmarkManagerUseCase, SaveFailureIsForwardedVerbatim) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.build();
    fixture.benchmarks->nextSaveOutcome = {std::nullopt, BenchmarkSaveError::Conflict};

    const auto outcome = fixture.manager->save(savedBenchmark("b1"), std::nullopt);

    ASSERT_TRUE(outcome.error.has_value());
    EXPECT_EQ(*outcome.error, BenchmarkSaveError::Conflict);
}

TEST(BenchmarkManagerUseCase, ResolveForwardsToStatelessResolutionAndRetainsNothing) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = BenchmarkLibrarySnapshot{};
    fixture.resolution->resolveResult = {resolvedEntry("e1", "h1")};
    fixture.build();

    const auto working = savedBenchmark("draft-1");
    const auto resolved = fixture.manager->resolve(working);

    ASSERT_EQ(resolved.size(), 1U);
    EXPECT_EQ(resolved.front().state, ScenarioMatchState::Resolved);
    // The manager state never grows a copy of the supplied working benchmark.
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    EXPECT_TRUE(fixture.manager->state().library->entries.empty());
}

TEST(BenchmarkManagerUseCase, DeleteForwardsTheTokenAndReturnsTheOutcomeUnchanged) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.build();
    const BenchmarkEditToken token{BenchmarkId{"b1"}, "b1.json", "d-b1"};
    fixture.benchmarks->nextRemoveOutcome = {BenchmarkRemoveError::Conflict};

    const auto outcome = fixture.manager->deleteBenchmark(token);

    ASSERT_EQ(fixture.benchmarks->removeRequests.size(), 1U);
    EXPECT_EQ(fixture.benchmarks->removeRequests.front().filename, "b1.json");
    ASSERT_TRUE(outcome.error.has_value());
    EXPECT_EQ(*outcome.error, BenchmarkRemoveError::Conflict);
}

TEST(BenchmarkManagerUseCase, ImportPlaylistSeedBuildsATokenlessSeedAndLeasesItsId) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = BenchmarkLibrarySnapshot{};
    fixture.reader->nextResult = {PlaylistSeed{std::string{"Voltaic"}, {"a", "b"}, {2}}, std::nullopt};
    fixture.build();

    const auto result = fixture.manager->importPlaylistSeed("playlist.json");

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(fixture.reader->lastPath, "playlist.json");
    ASSERT_TRUE(result.seed.has_value());
    EXPECT_EQ(result.seed->benchmark.name, "Voltaic");
    ASSERT_EQ(result.seed->benchmark.uncategorized.size(), 2U);
    EXPECT_FALSE(result.seed->token.has_value());
    EXPECT_EQ(result.skippedDuplicateIndices, std::vector<int>{2});
    ASSERT_TRUE(fixture.resolution->editLease.has_value());
    EXPECT_EQ(*fixture.resolution->editLease, result.seed->benchmark.id);
}

TEST(BenchmarkManagerUseCase, ImportPlaylistFailureIsForwardedWithoutASeed) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = BenchmarkLibrarySnapshot{};
    fixture.reader->nextResult = {std::nullopt, PlaylistImportFailure::MalformedJson};
    fixture.build();

    const auto result = fixture.manager->importPlaylistSeed("playlist.json");

    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(*result.failure, PlaylistImportFailure::MalformedJson);
    EXPECT_FALSE(result.seed.has_value());
}

TEST(BenchmarkManagerUseCase, RefreshAndManagedDirectoryForwardToTheAcceptedService) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = BenchmarkLibrarySnapshot{};
    fixture.benchmarks->directory = "/custom/benchmarks";
    fixture.build();

    fixture.manager->refresh();
    EXPECT_EQ(fixture.benchmarks->refreshCount, 1);
    EXPECT_EQ(fixture.manager->managedDirectoryPath(), "/custom/benchmarks");
}

// ---- Task P3 behavioural coverage (production delivered under P2's --scope app gate) -----------

TEST(BenchmarkManagerUseCase, ResolveWorkingCopyIsStateless) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = BenchmarkLibrarySnapshot{};
    fixture.resolution->resolveResultById["draft-a"] = {resolvedEntry("ea", "ha")};
    fixture.resolution->resolveResultById["draft-b"] = {ambiguousEntry("eb", {"hb1", "hb2"})};
    fixture.build();

    Benchmark a = savedBenchmark("draft-a");
    a.name = "Working A";
    Benchmark b = savedBenchmark("draft-b");
    b.name = "Working B";

    const auto ra = fixture.manager->resolve(a);
    const auto rb = fixture.manager->resolve(b);

    ASSERT_EQ(ra.size(), 1U);
    EXPECT_EQ(ra.front().state, ScenarioMatchState::Resolved);
    ASSERT_EQ(rb.size(), 1U);
    EXPECT_EQ(rb.front().state, ScenarioMatchState::Ambiguous);

    // The exact supplied values reached the stateless resolver, in order.
    ASSERT_EQ(fixture.resolution->resolvedBenchmarks.size(), 2U);
    EXPECT_EQ(fixture.resolution->resolvedBenchmarks[0].name, "Working A");
    EXPECT_EQ(fixture.resolution->resolvedBenchmarks[1].name, "Working B");

    // No later manager state or callback exposes either working copy.
    ASSERT_TRUE(fixture.manager->state().library.has_value());
    EXPECT_TRUE(fixture.manager->state().library->entries.empty());
}

TEST(BenchmarkManagerUseCase, CloseEditorReleasesLeaseExactlyOnce) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.build();
    fixture.manager->openBenchmark(BenchmarkId{"b1"});
    const auto leaseSetsAfterOpen = fixture.resolution->setEditLeaseCount;

    fixture.manager->closeEditor();

    EXPECT_FALSE(fixture.resolution->editLease.has_value());
    EXPECT_EQ(fixture.resolution->setEditLeaseCount, leaseSetsAfterOpen + 1);
}

TEST(BenchmarkManagerUseCase, SaveFailurePreservesTheEditorLease) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.build();
    fixture.manager->openBenchmark(BenchmarkId{"b1"});
    ASSERT_TRUE(fixture.resolution->editLease.has_value());
    fixture.benchmarks->nextSaveOutcome = {std::nullopt, BenchmarkSaveError::Conflict};

    const auto outcome = fixture.manager->save(savedBenchmark("b1"),
                                               BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d-b1"});

    ASSERT_TRUE(outcome.error.has_value());
    ASSERT_TRUE(fixture.resolution->editLease.has_value());
    EXPECT_EQ(fixture.resolution->editLease->value, "b1");
}

TEST(BenchmarkManagerUseCase, SaveSuccessReturnsAdmittedTokenWithoutAnInterveningAutomaticWrite) {
    Fixture fixture;
    fixture.benchmarks->snapshotValue = snapshotOf(savedBenchmark("b1"));
    fixture.build();
    fixture.benchmarks->nextSaveOutcome = {BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d-b1-2"},
                                           std::nullopt};

    const auto outcome = fixture.manager->save(savedBenchmark("b1"),
                                               BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d-b1"});

    ASSERT_TRUE(outcome.ok());
    EXPECT_EQ(outcome.token->digest, "d-b1-2");
    EXPECT_TRUE(fixture.benchmarks->batchRequests.empty());  // manager issued no reconciliation batch
}
