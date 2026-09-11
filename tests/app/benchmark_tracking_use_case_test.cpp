#include <gtest/gtest.h>

#include <memory>

#include "benchmark_builders.h"
#include "fake_benchmark_store.h"
#include "fake_profile_service.h"
#include "data/benchmarks_service.h"
#include "usecases/benchmark_resolution_use_case.h"
#include "usecases/benchmark_tracking_use_case.h"

using namespace ksv;
using namespace ksv::application;
using namespace ksv::data;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    constexpr long long kDay = 86'400'000LL;

    std::vector<std::pair<std::string, double> > rungs() {
        return {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}};
    }

    Benchmark benchmark(const std::string &id) {
        Benchmark value;
        value.id = BenchmarkId{id};
        value.name = "Tracked";
        value.tiers = {benchmarkTier("bronze"), benchmarkTier("silver"), benchmarkTier("gold")};
        value.uncategorized = {benchmarkEntry("e1", "Alpha", std::string{"h1"}, rungs()),
                               benchmarkEntry("e2", "Beta", std::string{"h2"}, rungs())};
        return value;
    }

    BenchmarkFileEntry fileFor(const Benchmark &value, const std::string &filename) {
        return {filename, "d-" + filename, LoadedBenchmark{value, validateBenchmark(value)}};
    }

    struct Fixture {
        std::shared_ptr<FakeBenchmarkStore> repo = std::make_shared<FakeBenchmarkStore>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<BenchmarksService> benchmarks;
        std::shared_ptr<BenchmarkResolutionUseCase> resolution;
        std::unique_ptr<BenchmarkTrackingUseCase> tracking;

        void known(const std::string &name, const std::string &hash) {
            profile->scenarios.push_back({name, hash});
        }

        void build() {
            benchmarks = std::make_shared<BenchmarksService>(repo);
            resolution = std::make_shared<BenchmarkResolutionUseCase>(benchmarks, profile);
            tracking = std::make_unique<BenchmarkTrackingUseCase>(benchmarks, resolution, profile);
        }
    };

    BenchmarkLibrarySnapshot snapshotFor(const Benchmark &value) {
        BenchmarkLibrarySnapshot snapshot;
        snapshot.entries.push_back(fileFor(value, value.id.value + ".json"));
        return snapshot;
    }
}

TEST(BenchmarkTracking, StartsWithNoSelection) {
    Fixture fixture;
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.build();
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::NoSelection);
    EXPECT_FALSE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->projection(), nullptr);
}

TEST(BenchmarkTracking, SelectingTrackableBenchmarkProducesProjection) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Ready);
    ASSERT_NE(fixture.tracking->projection(), nullptr);
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.tracking->projection()->attainedRank->value, "silver");
    EXPECT_DOUBLE_EQ(fixture.tracking->projection()->totalPlaytimeSeconds, 90.0);
}

TEST(BenchmarkTracking, RequestsUniqueResolvedHashesAndThreeDayRollingWindow) {
    Fixture fixture;
    auto value = benchmark("b1");
    value.uncategorized.push_back(benchmarkEntry("e3", "Again", std::string{"h1"}, rungs()));
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.profile->requested_fact_scenarios.size(), 2U);
    EXPECT_EQ(fixture.profile->requested_rolling_scenarios.size(), 2U);
    EXPECT_EQ(fixture.profile->filtered_rolling_window_days, 3);
}

TEST(BenchmarkTracking, DoesNotRequestUnresolvedEntries) {
    Fixture fixture;
    auto value = benchmark("b1");
    value.uncategorized[1].hash.reset();
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.profile->requested_fact_scenarios.size(), 1U);
    EXPECT_EQ(fixture.profile->requested_fact_scenarios.front().hash, "h1");
}

TEST(BenchmarkTracking, CopiesRollingPlaytime) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->filtered_rolling_time_average = {{std::chrono::sys_days{std::chrono::days{100}}, 45.0}};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_NE(fixture.tracking->projection(), nullptr);
    EXPECT_DOUBLE_EQ(fixture.tracking->projection()->rollingPlaytime.front().second, 45.0);
}

TEST(BenchmarkTracking, MissingProblemAndDuplicateDefinitionsAreUnavailable) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("b1"), "a.json"));
    snapshot.entries.push_back(fileFor(benchmark("b1"), "b.json"));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
    EXPECT_EQ(fixture.tracking->projection(), nullptr);
    fixture.tracking->select(BenchmarkId{"missing"});
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
}

TEST(BenchmarkTracking, ProblemDefinitionIsUnavailable) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back({"broken.json", "digest",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, BenchmarkId{"b1"}, {}}});
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
    EXPECT_EQ(fixture.tracking->projection(), nullptr);
}

TEST(BenchmarkTracking, ClearAndRepeatSelectionHaveStableNotifications) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(notifications, 0);
    fixture.tracking->clearSelection();
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::NoSelection);
}

TEST(BenchmarkTracking, IncompleteBenchmarkStillProjectsSafeFacts) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    auto value = benchmark("b1");
    value.uncategorized[1].thresholds.clear();
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_NE(fixture.tracking->projection(), nullptr);
    EXPECT_EQ(fixture.tracking->projection()->completeness, Completeness::Incomplete);
    EXPECT_FALSE(fixture.tracking->projection()->attainedRank);
    EXPECT_DOUBLE_EQ(fixture.tracking->projection()->totalPlaytimeSeconds, 60.0);
}

TEST(BenchmarkTrackingCache, ReusesPriorSelectionUntilRevisionChanges) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("b1"), "b1.json"));
    snapshot.entries.push_back(fileFor(benchmark("b2"), "b2.json"));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    const int first = fixture.profile->fact_request_count;
    fixture.tracking->select(BenchmarkId{"b2"});
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.profile->fact_request_count, first + 1);
}

// The other half of the cache contract: an entry cached before a publication must not be served
// after it, or an edit would keep showing history the current definition would never produce.
TEST(BenchmarkTrackingCache, DoesNotServeAnEntryCachedBeforeALibraryChange) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("b1"), "b1.json"));
    snapshot.entries.push_back(fileFor(benchmark("b2"), "b2.json"));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    fixture.tracking->select(BenchmarkId{"b2"});

    fixture.benchmarks->refresh();
    const int afterRefresh = fixture.profile->fact_request_count;
    fixture.tracking->select(BenchmarkId{"b1"});

    EXPECT_GT(fixture.profile->fact_request_count, afterRefresh);
}

TEST(BenchmarkTrackingCache, ProfileAndLibraryChangesReevaluateAndRemovalIsUnavailable) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    const int selected = fixture.profile->fact_request_count;
    fixture.profile->notifyProfileChanged();
    EXPECT_GT(fixture.profile->fact_request_count, selected);
    const int changed = fixture.profile->fact_request_count;
    fixture.benchmarks->refresh();
    EXPECT_GT(fixture.profile->fact_request_count, changed);
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.benchmarks->refresh();
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Unavailable);
}

TEST(BenchmarkTrackingCache, DefinitionEditReconstructsTheCachedProjection) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 150.0F, 10.0F),
                                  benchmarkFact("h2", 101 * kDay, 150.0F, 10.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_NE(fixture.tracking->projection(), nullptr);
    ASSERT_TRUE(fixture.tracking->projection()->averageRank);
    EXPECT_DOUBLE_EQ(*fixture.tracking->projection()->averageRank, 1.5);

    auto edited = benchmark("b1");
    const std::vector<std::pair<std::string, double> > halved{
        {"bronze", 50.0}, {"silver", 100.0}, {"gold", 150.0}};
    edited.uncategorized[0] = benchmarkEntry("e1", "Alpha", std::string{"h1"}, halved);
    edited.uncategorized[1] = benchmarkEntry("e2", "Beta", std::string{"h2"}, halved);
    fixture.repo->nextScan = {snapshotFor(edited), std::nullopt};
    fixture.benchmarks->refresh();

    ASSERT_NE(fixture.tracking->projection(), nullptr);
    ASSERT_TRUE(fixture.tracking->projection()->averageRank);
    EXPECT_DOUBLE_EQ(*fixture.tracking->projection()->averageRank, 3.0);
    ASSERT_EQ(fixture.tracking->projection()->averageRankHistory.size(), 2U);
    EXPECT_DOUBLE_EQ(fixture.tracking->projection()->averageRankHistory.back().averageRank, 3.0);
}

// ---- Task A1: one coherent workspace snapshot ------------------------------------------------

namespace {
    BenchmarkFileEntry loadedEntry(const Benchmark &value, const std::string &filename) {
        return {filename, "d-" + filename, LoadedBenchmark{value, validateBenchmark(value)}};
    }

    BenchmarkFileEntry problemEntry(const std::string &filename, BenchmarkFileProblem problem,
                                    std::optional<std::string> name, std::optional<BenchmarkId> id,
                                    std::optional<int> schemaVersion) {
        return {filename, "d-" + filename,
                ProblemBenchmark{problem, std::move(name), std::move(id), schemaVersion}};
    }

    Benchmark incompleteBenchmark(const std::string &id) {
        auto value = benchmark(id);
        value.uncategorized[1].thresholds.clear();
        return value;
    }
}

TEST(BenchmarkTrackingWorkspaceSnapshot, StartupPublishesNoSelectionWithChoicesInRepositoryOrder) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(loadedEntry(benchmark("b1"), "b1.json"));
    snapshot.entries.push_back(problemEntry("broken.json", BenchmarkFileProblem::Invalid,
                                            std::string{"Broken"}, BenchmarkId{"b2"}, std::nullopt));
    snapshot.entries.push_back(loadedEntry(benchmark("b3"), "b3.json"));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::NoSelection);
    EXPECT_FALSE(snap.selectedId.has_value());
    EXPECT_FALSE(snap.selectedLoaded.has_value());
    EXPECT_FALSE(snap.projection.has_value());
    EXPECT_EQ(snap.libraryRevision, fixture.benchmarks->revision());
    ASSERT_EQ(snap.choices.size(), 3U);
    EXPECT_EQ(snap.choices[0].filename, "b1.json");
    EXPECT_EQ(snap.choices[1].filename, "broken.json");
    EXPECT_EQ(snap.choices[2].filename, "b3.json");
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ChoicesCarryClassificationSelectabilityNameAndSchemaVersion) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(loadedEntry(benchmark("trackable"), "trackable.json"));
    snapshot.entries.push_back(loadedEntry(incompleteBenchmark("incomplete"), "incomplete.json"));
    snapshot.entries.push_back(problemEntry("invalid.json", BenchmarkFileProblem::Invalid,
                                            std::string{"Broken Bench"}, BenchmarkId{"bad"}, std::nullopt));
    snapshot.entries.push_back(problemEntry("legacy.json", BenchmarkFileProblem::Unsupported,
                                            std::string{"Legacy Bench"}, BenchmarkId{"old"}, 2));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();

    const auto &choices = fixture.tracking->snapshot().choices;
    ASSERT_EQ(choices.size(), 4U);

    EXPECT_EQ(choices[0].classification, BenchmarkChoiceClassification::Trackable);
    EXPECT_TRUE(choices[0].selectable);
    EXPECT_EQ(choices[0].displayName, "Tracked");
    ASSERT_TRUE(choices[0].id.has_value());
    EXPECT_EQ(choices[0].id->value, "trackable");
    EXPECT_FALSE(choices[0].schemaVersion.has_value());

    EXPECT_EQ(choices[1].classification, BenchmarkChoiceClassification::Incomplete);
    EXPECT_TRUE(choices[1].selectable);

    EXPECT_EQ(choices[2].classification, BenchmarkChoiceClassification::Invalid);
    EXPECT_FALSE(choices[2].selectable);
    EXPECT_EQ(choices[2].displayName, "Broken Bench");
    ASSERT_TRUE(choices[2].id.has_value());
    EXPECT_EQ(choices[2].id->value, "bad");

    EXPECT_EQ(choices[3].classification, BenchmarkChoiceClassification::Unsupported);
    EXPECT_FALSE(choices[3].selectable);
    EXPECT_EQ(choices[3].displayName, "Legacy Bench");
    ASSERT_TRUE(choices[3].schemaVersion.has_value());
    EXPECT_EQ(*choices[3].schemaVersion, 2);
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ProblemEntryWithoutParsedNameUsesFilenameAsSafeDisplayName) {
    Fixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(problemEntry("mystery.json", BenchmarkFileProblem::Invalid,
                                            std::nullopt, std::nullopt, std::nullopt));
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.build();

    const auto &choices = fixture.tracking->snapshot().choices;
    ASSERT_EQ(choices.size(), 1U);
    EXPECT_FALSE(choices[0].id.has_value());
    EXPECT_EQ(choices[0].displayName, "mystery.json");
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ReadyTrackableSnapshotCarriesDefinitionAndProjectionInOneRevision) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();

    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });
    fixture.tracking->select(BenchmarkId{"b1"});

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    ASSERT_TRUE(snap.selectedLoaded.has_value());
    EXPECT_EQ(snap.selectedLoaded->benchmark.id.value, "b1");
    EXPECT_EQ(snap.selectedLoaded->completeness.completeness, Completeness::Trackable);
    EXPECT_TRUE(snap.selectedLoaded->completeness.issues.empty());
    ASSERT_TRUE(snap.projection.has_value());
    EXPECT_EQ(snap.projection->benchmarkId.value, "b1");
    ASSERT_TRUE(snap.projection->attainedRank.has_value());
    EXPECT_EQ(snap.projection->attainedRank->value, "silver");
    EXPECT_EQ(snap.libraryRevision, fixture.benchmarks->revision());
}

TEST(BenchmarkTrackingWorkspaceSnapshot, ReadyIncompleteSnapshotCarriesFullCompletenessIssueVector) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    const auto value = incompleteBenchmark("b1");
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F)};
    fixture.build();

    fixture.tracking->select(BenchmarkId{"b1"});

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snap.selectedLoaded.has_value());
    EXPECT_EQ(snap.selectedLoaded->completeness.completeness, Completeness::Incomplete);
    const auto expectedIssues = validateBenchmark(value).issues;
    ASSERT_FALSE(expectedIssues.empty());
    EXPECT_EQ(snap.selectedLoaded->completeness.issues.size(), expectedIssues.size());
    ASSERT_TRUE(snap.projection.has_value());
    EXPECT_EQ(snap.projection->completeness, Completeness::Incomplete);
}

// ---- Task A2: selection lifecycle across deletion, conflict, and recovery --------------------

TEST(BenchmarkTrackingWorkspaceLifecycle, SelectingAlreadySelectedIdIsANotificationNoOp) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});

    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });
    fixture.tracking->select(BenchmarkId{"b1"});

    EXPECT_EQ(notifications, 0);
    EXPECT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);
}

TEST(BenchmarkTrackingWorkspaceLifecycle, ClearSelectionReturnsToNoSelectionAndDropsDerivedState) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    fixture.tracking->clearSelection();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::NoSelection);
    EXPECT_FALSE(snap.selectedId.has_value());
    EXPECT_FALSE(snap.selectedLoaded.has_value());
    EXPECT_FALSE(snap.projection.has_value());
    EXPECT_EQ(fixture.tracking->projection(), nullptr);
}

TEST(BenchmarkTrackingWorkspaceLifecycle, SelectingUnknownIdPublishesUnavailableWithoutARetainedProjection) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_TRUE(fixture.tracking->snapshot().projection.has_value());

    fixture.tracking->select(BenchmarkId{"ghost"});

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Unavailable);
    EXPECT_FALSE(snap.projection.has_value());
    EXPECT_FALSE(snap.selectedLoaded.has_value());
    EXPECT_EQ(fixture.tracking->projection(), nullptr);
}

// The selected file is removed entirely on the next scan. The workspace must keep the selected id
// and the last name the user saw, but drop every derived field, so the Unavailable screen can name
// the benchmark that vanished without showing stale ranks or history.
TEST(BenchmarkTrackingWorkspaceLifecycle, SelectedDeletionGoesUnavailableRetainingIdAndLastKnownName) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);
    ASSERT_EQ(fixture.tracking->snapshot().lastKnownSelectedDisplayName, "Tracked");

    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.benchmarks->refresh();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Unavailable);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    EXPECT_EQ(snap.lastKnownSelectedDisplayName, "Tracked");
    EXPECT_FALSE(snap.selectedLoaded.has_value());
    EXPECT_FALSE(snap.projection.has_value());
    EXPECT_EQ(fixture.tracking->projection(), nullptr);
}

// The file survives but now fails to parse and exposes no embedded id, so it never appears as a
// choice the current lookup can read a name from. The last-known name must still be retained.
TEST(BenchmarkTrackingWorkspaceLifecycle, SelectedBecomingUnparseableProblemRetainsLastKnownName) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    BenchmarkLibrarySnapshot broken;
    broken.entries.push_back({"b1.json", "digest",
                              ProblemBenchmark{BenchmarkFileProblem::Invalid, std::nullopt,
                                               std::nullopt, std::nullopt}});
    fixture.repo->nextScan = {broken, std::nullopt};
    fixture.benchmarks->refresh();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Unavailable);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    EXPECT_EQ(snap.lastKnownSelectedDisplayName, "Tracked");
    EXPECT_FALSE(snap.selectedLoaded.has_value());
    EXPECT_FALSE(snap.projection.has_value());
}

TEST(BenchmarkTrackingWorkspaceLifecycle, SelectedDuplicateIdConflictGoesUnavailableAndClearsDerivedState) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    BenchmarkLibrarySnapshot clash;
    clash.entries.push_back(loadedEntry(benchmark("b1"), "a.json"));
    clash.entries.push_back(loadedEntry(benchmark("b1"), "b.json"));
    fixture.repo->nextScan = {clash, std::nullopt};
    fixture.benchmarks->refresh();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Unavailable);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    EXPECT_EQ(snap.lastKnownSelectedDisplayName, "Tracked");
    EXPECT_FALSE(snap.selectedLoaded.has_value());
    EXPECT_FALSE(snap.projection.has_value());
    EXPECT_EQ(fixture.tracking->projection(), nullptr);
}

// A directory-level scan failure is not a deletion: the library keeps its last accepted snapshot,
// so the tracking workspace must keep showing its projection rather than dropping to Unavailable.
TEST(BenchmarkTrackingWorkspaceLifecycle, WholeDirectoryRefreshFailureKeepsAcceptedProjectionVisible) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    fixture.repo->nextScan = {std::nullopt, BenchmarkScanFailure::DirectoryUnavailable};
    fixture.benchmarks->refresh();

    ASSERT_TRUE(fixture.benchmarks->lastRefreshFailed());
    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snap.selectedLoaded.has_value());
    ASSERT_TRUE(snap.projection.has_value());
    EXPECT_EQ(snap.projection->benchmarkId.value, "b1");
    EXPECT_NE(fixture.tracking->projection(), nullptr);
}

TEST(BenchmarkTrackingWorkspaceLifecycle, ProfileChangeInvalidatesProjectionButKeepsChoicesAndSelection) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    const auto choicesBefore = fixture.tracking->snapshot().choices.size();
    const auto selectedBefore = fixture.tracking->snapshot().selectedId;
    const int factsBefore = fixture.profile->fact_request_count;

    fixture.profile->notifyProfileChanged();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_GT(fixture.profile->fact_request_count, factsBefore);
    EXPECT_EQ(snap.choices.size(), choicesBefore);
    EXPECT_EQ(snap.selectedId, selectedBefore);
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
}

TEST(BenchmarkTrackingWorkspaceLifecycle, UnavailableSelectionAutoRecoversWhenSameIdReturns) {
    Fixture fixture;
    fixture.known("Alpha", "h1");
    fixture.known("Beta", "h2");
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F)};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Ready);

    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.benchmarks->refresh();
    ASSERT_EQ(fixture.tracking->snapshot().availability, BenchmarkTrackingState::Unavailable);

    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.benchmarks->refresh();

    const auto &snap = fixture.tracking->snapshot();
    EXPECT_EQ(snap.availability, BenchmarkTrackingState::Ready);
    ASSERT_TRUE(snap.selectedId.has_value());
    EXPECT_EQ(snap.selectedId->value, "b1");
    ASSERT_TRUE(snap.projection.has_value());
    EXPECT_EQ(snap.projection->benchmarkId.value, "b1");
}
