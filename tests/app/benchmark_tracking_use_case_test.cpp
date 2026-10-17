#include <gtest/gtest.h>

#include <memory>

#include "benchmark_builders.h"
#include "fake_benchmark_store.h"
#include "fake_day_source.h"
#include "fake_graph_configuration_use_case.h"
#include "fake_profile_service.h"
#include "fake_settings_service.h"
#include "user_profile.h"
#include "data/benchmarks_service.h"
#include "usecases/benchmark_resolution_use_case.h"
#include "usecases/benchmark_tracking_use_case.h"
#include "usecases/recent_benchmarks.h"

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
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();
        std::shared_ptr<FakeDaySource> days = std::make_shared<FakeDaySource>();
        std::shared_ptr<BenchmarksService> benchmarks;
        std::shared_ptr<BenchmarkResolutionUseCase> resolution;
        std::unique_ptr<BenchmarkTrackingUseCase> tracking;

        GraphPreferences &playtimePreferences() {
            return configuration->configuration.preferences(GraphKey::BenchmarkPlaytime);
        }

        void commitPlaytime(const GraphKey key = GraphKey::BenchmarkPlaytime) {
            ++configuration->revision;
            configuration->publish({key});
        }

        void known(const std::string &name, const std::string &hash) {
            profile->scenarios.push_back({name, hash});
        }

        void build() {
            benchmarks = std::make_shared<BenchmarksService>(repo);
            resolution = std::make_shared<BenchmarkResolutionUseCase>(benchmarks, profile);
            tracking = std::make_unique<BenchmarkTrackingUseCase>(benchmarks, resolution, profile, configuration, days);
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

// ---- default selection and remembered choice ------------------------------------------

namespace {
    struct RememberingFixture {
        std::shared_ptr<FakeBenchmarkStore> repo = std::make_shared<FakeBenchmarkStore>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeSettingsService> settings = std::make_shared<FakeSettingsService>();
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();
        std::shared_ptr<FakeDaySource> days = std::make_shared<FakeDaySource>();
        std::shared_ptr<BenchmarksService> benchmarks;
        std::shared_ptr<BenchmarkResolutionUseCase> resolution;
        std::shared_ptr<RecentBenchmarks> recent;
        std::unique_ptr<BenchmarkTrackingUseCase> tracking;

        void build(const std::vector<std::string> &ids) {
            BenchmarkLibrarySnapshot snapshot;
            for (const auto &id: ids) snapshot.entries.push_back(fileFor(benchmark(id), id + ".json"));
            repo->nextScan = {snapshot, std::nullopt};
            benchmarks = std::make_shared<BenchmarksService>(repo);
            resolution = std::make_shared<BenchmarkResolutionUseCase>(benchmarks, profile);
            recent = std::make_shared<RecentBenchmarks>(settings);
            tracking = std::make_unique<BenchmarkTrackingUseCase>(benchmarks, resolution, profile, configuration, days,
                                                                  recent);
        }

        std::optional<std::size_t> recencyOf(const std::string &id) const {
            for (const auto &choice: tracking->snapshot().choices)
                if (choice.id && choice.id->value == id) return choice.recency;
            return std::nullopt;
        }
    };

    using Ids = std::vector<std::string>;
}

TEST(BenchmarkTrackingDefaultSelection, NothingIsSelectedUntilAskedEvenWithASingleBenchmark) {
    RememberingFixture fixture;
    fixture.build({"only"});
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::NoSelection);
}

TEST(BenchmarkTrackingDefaultSelection, SelectsTheOnlyBenchmark) {
    RememberingFixture fixture;
    fixture.build({"only"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "only");
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Ready);
    EXPECT_EQ(notifications, 1);
}

TEST(BenchmarkTrackingDefaultSelection, DoesNotGuessBetweenSeveralBenchmarks) {
    RememberingFixture fixture;
    fixture.build({"b1", "b2"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->selectDefault();

    EXPECT_FALSE(fixture.tracking->selected());
    EXPECT_EQ(notifications, 0);
}

TEST(BenchmarkTrackingDefaultSelection, IgnoresProblemFilesWhenCountingTheOnlyBenchmark) {
    RememberingFixture fixture;
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("good"), "good.json"));
    snapshot.entries.push_back({"broken.json", "digest",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, BenchmarkId{"bad"}, {}}});
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.benchmarks = std::make_shared<BenchmarksService>(fixture.repo);
    fixture.resolution = std::make_shared<BenchmarkResolutionUseCase>(fixture.benchmarks, fixture.profile);
    fixture.recent = std::make_shared<RecentBenchmarks>(fixture.settings);
    fixture.tracking = std::make_unique<BenchmarkTrackingUseCase>(fixture.benchmarks, fixture.resolution,
                                                                  fixture.profile, fixture.configuration,
                                                                  fixture.days, fixture.recent);

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "good");
}

TEST(BenchmarkTrackingDefaultSelection, RestoresTheRememberedBenchmarkAmongSeveral) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b2"};
    fixture.build({"b1", "b2", "b3"});

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "b2");
    EXPECT_EQ(fixture.tracking->state(), BenchmarkTrackingState::Ready);
}

TEST(BenchmarkTrackingDefaultSelection, FallsBackToTheOnlyBenchmarkWhenTheRememberedOneIsGone) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"deleted"};
    fixture.build({"only"});

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "only");
}

TEST(BenchmarkTrackingDefaultSelection, RememberedButMissingAmongSeveralSelectsNothing) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"deleted"};
    fixture.build({"b1", "b2"});

    fixture.tracking->selectDefault();

    EXPECT_FALSE(fixture.tracking->selected());
}

TEST(BenchmarkTrackingDefaultSelection, NeverReplacesAnExistingSelection) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b1"};
    fixture.build({"b1", "b2"});
    fixture.tracking->select(BenchmarkId{"b2"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->selectDefault();

    EXPECT_EQ(fixture.tracking->selected()->value, "b2");
    EXPECT_EQ(notifications, 0);
}

TEST(BenchmarkTrackingDefaultSelection, SkipsAFrontIdThatIsNoLongerSelectableAndTakesTheNext) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"deleted", "b3", "b1"};
    fixture.build({"b1", "b2", "b3"});

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "b3");
}

TEST(BenchmarkTrackingDefaultSelection, SkipsARecentIdThatNamesAProblemFile) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"bad", "b1"};
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back(fileFor(benchmark("b1"), "b1.json"));
    snapshot.entries.push_back(fileFor(benchmark("b2"), "b2.json"));
    snapshot.entries.push_back({"broken.json", "digest",
                                ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, BenchmarkId{"bad"}, {}}});
    fixture.repo->nextScan = {snapshot, std::nullopt};
    fixture.benchmarks = std::make_shared<BenchmarksService>(fixture.repo);
    fixture.resolution = std::make_shared<BenchmarkResolutionUseCase>(fixture.benchmarks, fixture.profile);
    fixture.recent = std::make_shared<RecentBenchmarks>(fixture.settings);
    fixture.tracking = std::make_unique<BenchmarkTrackingUseCase>(fixture.benchmarks, fixture.resolution,
                                                                  fixture.profile, fixture.configuration,
                                                                  fixture.days, fixture.recent);

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "b1");
}

TEST(BenchmarkTrackingRecent, SelectMovesItsIdToTheFrontAndNotifiesExactlyOnce) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b1", "b2"};
    fixture.build({"b1", "b2", "b3"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->select(BenchmarkId{"b3"});

    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.settings->recentBenchmarkIds, (Ids{"b3", "b1", "b2"}));
    EXPECT_EQ(fixture.recencyOf("b3"), 0U);
    EXPECT_EQ(fixture.recencyOf("b1"), 1U);
    EXPECT_EQ(fixture.recencyOf("b2"), 2U);
}

TEST(BenchmarkTrackingRecent, SelectingAnAlreadyFrontIdNotifiesOnce) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b2", "b1"};
    fixture.build({"b1", "b2"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.tracking->select(BenchmarkId{"b2"});

    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.settings->recentBenchmarkIds, (Ids{"b2", "b1"}));
}

TEST(BenchmarkTrackingRecent, ClearSelectionLeavesTheListUnchanged) {
    RememberingFixture fixture;
    fixture.build({"b1", "b2"});
    fixture.tracking->select(BenchmarkId{"b2"});
    const int writes = fixture.settings->recentBenchmarkIdsWrites;

    fixture.tracking->clearSelection();

    EXPECT_EQ(fixture.settings->recentBenchmarkIds, (Ids{"b2"}));
    EXPECT_EQ(fixture.settings->recentBenchmarkIdsWrites, writes);
    EXPECT_EQ(fixture.recencyOf("b2"), 0U);
}

TEST(BenchmarkTrackingRecent, ChoicesCarryTheirIndexInTheListOrNothing) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"gone", "b3", "b1"};
    fixture.build({"b1", "b2", "b3"});

    EXPECT_EQ(fixture.recencyOf("b3"), 1U);
    EXPECT_EQ(fixture.recencyOf("b1"), 2U);
    EXPECT_EQ(fixture.recencyOf("b2"), std::nullopt);
}

TEST(BenchmarkTrackingRecent, ACreateMadeElsewhereReordersTheChoicesAndNotifiesOnce) {
    RememberingFixture fixture;
    fixture.settings->recentBenchmarkIds = {"b1"};
    fixture.build({"b1", "b2"});
    int notifications = 0;
    fixture.tracking->onChanged([&] { ++notifications; });

    fixture.recent->markCreated("b2");

    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(fixture.recencyOf("b1"), 0U);
    EXPECT_EQ(fixture.recencyOf("b2"), 1U);
}

TEST(BenchmarkTrackingRecent, WithoutAListChoicesHaveNoRecency) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});

    ASSERT_EQ(fixture.tracking->snapshot().choices.size(), 1U);
    EXPECT_FALSE(fixture.tracking->snapshot().choices.front().recency);
}

// The restart round trip: a choice made in one session is what the next session restores.
TEST(BenchmarkTrackingDefaultSelection, ChoiceSurvivesIntoANewSession) {
    RememberingFixture first;
    first.build({"b1", "b2", "b3"});
    first.tracking->select(BenchmarkId{"b3"});

    RememberingFixture second;
    second.settings = first.settings;
    second.build({"b1", "b2", "b3"});
    EXPECT_FALSE(second.tracking->selected()) << "nothing is selected before the workspace asks";
    second.tracking->selectDefault();

    ASSERT_TRUE(second.tracking->selected());
    EXPECT_EQ(second.tracking->selected()->value, "b3");
}

TEST(BenchmarkTrackingDefaultSelection, WithoutSettingsOnlyTheSoleBenchmarkRuleApplies) {
    Fixture fixture;
    fixture.repo->nextScan = {snapshotFor(benchmark("only")), std::nullopt};
    fixture.build();

    fixture.tracking->selectDefault();

    ASSERT_TRUE(fixture.tracking->selected());
    EXPECT_EQ(fixture.tracking->selected()->value, "only");
}

TEST(BenchmarkTracking, RequestsEveryExplicitLinkedHashOnce) {
    Fixture fixture;
    auto value = benchmark("b1");
    value.uncategorized[0].additionalHashes = {"new", "h2", "new"};
    fixture.repo->nextScan = {snapshotFor(value), std::nullopt};
    fixture.build();
    fixture.tracking->select(BenchmarkId{"b1"});
    EXPECT_EQ(fixture.profile->requested_fact_scenarios.size(), 3U);
    EXPECT_EQ(fixture.profile->requested_rolling_scenarios.size(), 3U);
    EXPECT_TRUE(std::ranges::any_of(fixture.profile->requested_fact_scenarios,
                                  [](const auto &scenario) { return scenario.hash == "new"; }));
}

// ---- playtime window and range ----------------------------------------------------------

namespace {
    using Series = std::vector<std::pair<std::chrono::sys_days, double> >;

    std::chrono::sys_days epochDay(const long long day) { return std::chrono::sys_days{std::chrono::days{day}}; }

    // Real rolling accounting behind the fake, over hash "h1" runs on the given days.
    void useRealAccounting(Fixture &fixture, const std::vector<std::pair<long long, float> > &dailyRuns) {
        auto profile = std::make_shared<UserProfile>();
        for (const auto &[day, seconds]: dailyRuns) {
            Run run;
            run.run_id.scenario_id = {"Alpha", "h1"};
            run.run_id.start_time = day * kDay;
            run.scenario_length = seconds;
            profile->addRun(run);
        }
        fixture.profile->filtered_rolling_by_window = [profile](const int window) {
            return profile->getRollingTimeAverage(window);
        };
        for (const auto &[day, seconds]: dailyRuns)
            fixture.profile->run_facts.push_back(benchmarkFact("h1", day * kDay, 250.0F, seconds));
    }

    void expectSameSummary(const BenchmarkProjection &left, const BenchmarkProjection &right) {
        EXPECT_EQ(left.attainedRank, right.attainedRank);
        EXPECT_EQ(left.nextTier, right.nextTier);
        EXPECT_EQ(left.averageRank, right.averageRank);
        EXPECT_DOUBLE_EQ(left.totalPlaytimeSeconds, right.totalPlaytimeSeconds);
        ASSERT_EQ(left.scenarios.size(), right.scenarios.size());
        for (std::size_t index = 0; index < left.scenarios.size(); ++index) {
            EXPECT_EQ(left.scenarios[index].personalBest, right.scenarios[index].personalBest);
            EXPECT_EQ(left.scenarios[index].attainedTier, right.scenarios[index].attainedTier);
        }
        ASSERT_EQ(left.averageRankHistory.size(), right.averageRankHistory.size());
        for (std::size_t index = 0; index < left.averageRankHistory.size(); ++index) {
            EXPECT_EQ(left.averageRankHistory[index].day, right.averageRankHistory[index].day);
            EXPECT_EQ(left.averageRankHistory[index].averageRank, right.averageRankHistory[index].averageRank);
        }
    }

    struct PlaytimeFixture : Fixture {
        int tracked = 0;
        int playtimeOnly = 0;

        PlaytimeFixture() {
            known("Alpha", "h1");
            known("Beta", "h2");
            repo->nextScan = {snapshotFor(benchmark("b1")), std::nullopt};
        }

        void start() {
            build();
            tracking->select(BenchmarkId{"b1"});
            tracking->onChanged([this] { ++tracked; });
            tracking->onPlaytimeChanged([this] { ++playtimeOnly; });
        }
    };
}

TEST(BenchmarkTrackingPlaytime, RequestsTheCommittedWindowAndReportsIt) {
    PlaytimeFixture fixture;
    fixture.playtimePreferences().averagingWindowDays = 7;
    fixture.start();

    EXPECT_EQ(fixture.profile->filtered_rolling_window_days, 7);
    EXPECT_EQ(fixture.tracking->snapshot().playtime.windowDays, 7);
}

TEST(BenchmarkTrackingPlaytime, OneDayWindowYieldsTheDailyTotals) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
    fixture.playtimePreferences().averagingWindowDays = 1;
    fixture.start();

    EXPECT_EQ(fixture.tracking->projection()->rollingPlaytime,
              (Series{{epochDay(100), 60.0}, {epochDay(101), 30.0}, {epochDay(104), 90.0}}));
}

TEST(BenchmarkTrackingPlaytime, ChangingTheWindowRecalculatesOnlyPlaytimeAndNeverTheSummaries) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
    fixture.start();
    const auto before = *fixture.tracking->projection();
    const auto factRequests = fixture.profile->fact_request_count;
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;

    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.commitPlaytime();

    EXPECT_EQ(fixture.playtimeOnly, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests + 1);
    EXPECT_NE(fixture.tracking->projection()->rollingPlaytime, before.rollingPlaytime);
    expectSameSummary(before, *fixture.tracking->projection());
    expectSameSummary(before, *fixture.tracking->snapshot().projection);
    EXPECT_EQ(fixture.tracking->snapshot().projection->rollingPlaytime, fixture.tracking->projection()->rollingPlaytime);
}

TEST(BenchmarkTrackingPlaytime, ANarrowerRangeKeepsOverlappingValuesAndTheWindow) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}, {106, 30.0F}});
    fixture.playtimePreferences().averagingWindowDays = 4;
    fixture.start();
    const auto before = *fixture.tracking->projection();
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;

    fixture.days->setToday(epochDay(106));
    fixture.playtimePreferences().range = RangePreference{RecentDays{3}, false};
    fixture.commitPlaytime();

    const auto &narrowed = fixture.tracking->projection()->rollingPlaytime;
    ASSERT_EQ(narrowed.size(), 3U);
    EXPECT_EQ(narrowed, Series(before.rollingPlaytime.end() - 3, before.rollingPlaytime.end()));
    EXPECT_EQ(fixture.tracking->snapshot().playtime.windowDays, 4);
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval,
              std::optional(DayInterval{epochDay(104), epochDay(106)}));
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests);
    expectSameSummary(before, *fixture.tracking->projection());
}

TEST(BenchmarkTrackingPlaytime, TheFullExtentSurvivesNarrowing) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
    fixture.start();
    const auto fullExtent = fixture.tracking->snapshot().playtime.fullExtent;
    ASSERT_TRUE(fullExtent);

    fixture.playtimePreferences().range = RangePreference{RecentDays{1}, true};
    fixture.commitPlaytime();

    EXPECT_EQ(fixture.tracking->snapshot().playtime.fullExtent, fullExtent);
    EXPECT_EQ(fixture.tracking->projection()->rollingPlaytime.size(), 1U);
}

TEST(BenchmarkTrackingPlaytime, TheAnchorIsTheNewestContributingRunEvenWhenItImprovedNothing) {
    PlaytimeFixture fixture;
    fixture.profile->run_facts = {benchmarkFact("h1", 100 * kDay, 250.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 300.0F, 30.0F),
                                  benchmarkFact("h1", 110 * kDay, 10.0F, 30.0F)};
    fixture.days->setToday(epochDay(400));
    fixture.playtimePreferences().range = RangePreference{RecentDays{5}, true};
    fixture.start();

    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval,
              std::optional(DayInterval{epochDay(106), epochDay(110)}));
}

TEST(BenchmarkTrackingPlaytime, AnchoredRangeWithoutContributingRunsHasNoIntervalAndNoSamples) {
    PlaytimeFixture fixture;
    fixture.days->setToday(epochDay(400));
    fixture.playtimePreferences().range = RangePreference{RecentDays{5}, true};
    fixture.start();

    EXPECT_FALSE(fixture.tracking->snapshot().playtime.interval);
    EXPECT_TRUE(fixture.tracking->projection()->rollingPlaytime.empty());
}

TEST(BenchmarkTrackingPlaytime, ADayChangeReprojectsWithoutRecalculatingOrTouchingTheSummaries) {
    PlaytimeFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}});
    fixture.days->setToday(epochDay(101));
    fixture.playtimePreferences().range = RangePreference{RecentDays{2}, false};
    fixture.start();
    const auto before = *fixture.tracking->projection();
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;
    const auto factRequests = fixture.profile->fact_request_count;

    fixture.days->advance();

    EXPECT_EQ(fixture.playtimeOnly, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval,
              std::optional(DayInterval{epochDay(101), epochDay(102)}));
    EXPECT_EQ(fixture.tracking->projection()->rollingPlaytime.size(), 1U);
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    expectSameSummary(before, *fixture.tracking->projection());
}

TEST(BenchmarkTrackingPlaytime, ADayChangeIsIgnoredWhenTheRangeDoesNotFollowToday) {
    PlaytimeFixture fixture;
    fixture.start();

    fixture.days->advance();

    EXPECT_EQ(fixture.playtimeOnly, 0);
}

TEST(BenchmarkTrackingPlaytime, OtherGraphsAndUnselectedStatesAreNotRefreshed) {
    PlaytimeFixture fixture;
    fixture.start();

    fixture.commitPlaytime(GraphKey::BenchmarkRank);
    fixture.commitPlaytime(GraphKey::DailyPlaytime);
    EXPECT_EQ(fixture.playtimeOnly, 0);

    fixture.tracking->clearSelection();
    fixture.commitPlaytime();
    EXPECT_EQ(fixture.playtimeOnly, 0);
}

// ---- rank range ---------------------------------------------------------------------------

namespace {
    struct RankFixture : PlaytimeFixture {
        int rankOnly = 0;

        GraphPreferences &rankPreferences() {
            return configuration->configuration.preferences(GraphKey::BenchmarkRank);
        }

        void startRank() {
            start();
            tracking->onRankChanged([this] { ++rankOnly; });
        }

        void improvingThenFlatRuns() {
            profile->run_facts = {benchmarkFact("h1", 100 * kDay, 150.0F, 60.0F),
                                  benchmarkFact("h2", 101 * kDay, 250.0F, 30.0F),
                                  benchmarkFact("h1", 104 * kDay, 260.0F, 30.0F),
                                  benchmarkFact("h1", 110 * kDay, 10.0F, 30.0F)};
        }
    };
}

TEST(BenchmarkTrackingRank, TheDefaultRangeIsTheWholeHistory) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.startRank();

    EXPECT_EQ(fixture.tracking->snapshot().rank, RankRangeView{});
    EXPECT_EQ(fixture.tracking->projection()->averageRankHistory.size(), 3U);
}

TEST(BenchmarkTrackingRank, ARangeChangeReprojectsWithoutReevaluatingOrTouchingTheSummaries) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.startRank();
    const auto before = *fixture.tracking->projection();
    const auto factRequests = fixture.profile->fact_request_count;
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;

    fixture.rankPreferences().range = RangePreference{
        CustomDates{std::chrono::year_month_day{epochDay(102)}, std::chrono::year_month_day{epochDay(106)}}, false};
    fixture.commitPlaytime(GraphKey::BenchmarkRank);

    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(102), epochDay(106)}));
    EXPECT_FALSE(fixture.tracking->snapshot().rank.noRelevantHistory);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests);
    // The rank history stays whole: what came before the range is the rank carried into it.
    expectSameSummary(before, *fixture.tracking->projection());
    expectSameSummary(before, *fixture.tracking->snapshot().projection);
}

TEST(BenchmarkTrackingRank, TheAnchorIsTheNewestContributingRunNotTheLatestImprovement) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(400));
    fixture.rankPreferences().range = RangePreference{RecentDays{3}, true};
    fixture.startRank();

    ASSERT_EQ(fixture.tracking->projection()->averageRankHistory.back().day, epochDay(104));
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(108), epochDay(110)}));
}

TEST(BenchmarkTrackingRank, ALaterNonImprovingRunMovesTheAnchor) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(400));
    fixture.rankPreferences().range = RangePreference{RecentDays{3}, true};
    fixture.startRank();

    fixture.profile->run_facts.push_back(benchmarkFact("h2", 120 * kDay, 5.0F, 30.0F));
    fixture.profile->notifyProfileChanged();

    EXPECT_EQ(fixture.tracking->projection()->averageRankHistory.back().day, epochDay(104));
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(118), epochDay(120)}));
}

TEST(BenchmarkTrackingRank, AnAnchoredRangeWithoutContributingRunsHasNoRelevantHistory) {
    RankFixture fixture;
    fixture.days->setToday(epochDay(400));
    fixture.rankPreferences().range = RangePreference{RecentDays{5}, true};
    fixture.startRank();

    EXPECT_TRUE(fixture.tracking->snapshot().rank.noRelevantHistory);
    EXPECT_FALSE(fixture.tracking->snapshot().rank.interval);
}

TEST(BenchmarkTrackingRank, ATodayAnchoredRangeFollowsADayChangeWithoutReevaluating) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.days->setToday(epochDay(110));
    fixture.rankPreferences().range = RangePreference{RecentDays{5}, false};
    fixture.startRank();
    const auto factRequests = fixture.profile->fact_request_count;

    fixture.days->advance();

    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(107), epochDay(111)}));
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
}

TEST(BenchmarkTrackingRank, ADayChangeIsIgnoredWhenTheRankRangeDoesNotFollowToday) {
    RankFixture fixture;
    fixture.improvingThenFlatRuns();
    fixture.rankPreferences().range = RangePreference{RecentDays{5}, true};
    fixture.startRank();

    fixture.days->advance();

    EXPECT_EQ(fixture.rankOnly, 0);
}

TEST(BenchmarkTrackingRank, RankAndPlaytimeRangesNeverWriteEachOther) {
    RankFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}, {106, 30.0F}});
    fixture.days->setToday(epochDay(106));
    fixture.startRank();
    const auto playtime = fixture.tracking->projection()->rollingPlaytime;

    fixture.rankPreferences().range = RangePreference{RecentDays{2}, false};
    fixture.commitPlaytime(GraphKey::BenchmarkRank);
    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_FALSE(fixture.tracking->snapshot().playtime.interval);
    EXPECT_EQ(fixture.tracking->projection()->rollingPlaytime, playtime);

    fixture.playtimePreferences().range = RangePreference{RecentDays{4}, false};
    fixture.commitPlaytime();
    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.playtimeOnly, 1);
    EXPECT_EQ(fixture.tracking->snapshot().rank.interval, std::optional(DayInterval{epochDay(105), epochDay(106)}));
    EXPECT_EQ(fixture.tracking->snapshot().playtime.interval, std::optional(DayInterval{epochDay(103), epochDay(106)}));
}

TEST(BenchmarkTrackingRank, OtherGraphsAndUnselectedStatesAreNotRefreshed) {
    RankFixture fixture;
    fixture.startRank();

    fixture.commitPlaytime(GraphKey::BenchmarkPlaytime);
    fixture.commitPlaytime(GraphKey::ScenarioHistory);
    EXPECT_EQ(fixture.rankOnly, 0);

    fixture.tracking->clearSelection();
    fixture.commitPlaytime(GraphKey::BenchmarkRank);
    EXPECT_EQ(fixture.rankOnly, 0);
}

// ---- committed style ----------------------------------------------------------------------

namespace {
    struct StyleFixture : RankFixture {
        int style = 0;

        void startStyle() {
            startRank();
            tracking->onPresentationChanged([this] { ++style; });
        }

        void publishStyleEdits() {
            ++configuration->revision;
            configuration->publishDiff();
        }
    };
}

TEST(BenchmarkTrackingStyle, AStyleOnlyChangeNeverEvaluatesOrReprojectsEitherBenchmarkGraph) {
    StyleFixture fixture;
    useRealAccounting(fixture, {{100, 60.0F}, {101, 30.0F}, {104, 90.0F}});
    fixture.startStyle();
    const auto factRequests = fixture.profile->fact_request_count;
    const auto rollingRequests = fixture.profile->filtered_rolling_request_count;
    const auto before = *fixture.tracking->projection();

    fixture.playtimePreferences().appearance = {ConnectionMode::PointsOnly, true};
    fixture.playtimePreferences().seriesStyles["playtime"] = {RgbaColor{9, 9, 9, 255}, 8.0};
    fixture.rankPreferences().appearance.connection = ConnectionMode::Straight;
    fixture.rankPreferences().seriesStyles["rank"] = {RgbaColor{200, 0, 0, 255}, std::nullopt};
    fixture.publishStyleEdits();

    EXPECT_EQ(fixture.style, 1);
    EXPECT_EQ(fixture.tracked, 0);
    EXPECT_EQ(fixture.playtimeOnly, 0);
    EXPECT_EQ(fixture.rankOnly, 0);
    EXPECT_EQ(fixture.profile->fact_request_count, factRequests);
    EXPECT_EQ(fixture.profile->filtered_rolling_request_count, rollingRequests);
    expectSameSummary(before, *fixture.tracking->projection());
}

TEST(BenchmarkTrackingStyle, EachGraphReportsItsOwnCommittedStyle) {
    StyleFixture fixture;
    fixture.startStyle();

    fixture.playtimePreferences().appearance.connection = ConnectionMode::PointsOnly;
    fixture.rankPreferences().seriesStyles["rank"] = {RgbaColor{1, 1, 1, 255}, 5.0};
    fixture.publishStyleEdits();

    const auto rank = fixture.tracking->presentation(GraphKey::BenchmarkRank);
    const auto playtime = fixture.tracking->presentation(GraphKey::BenchmarkPlaytime);
    EXPECT_EQ(rank.appearance.connection, ConnectionMode::Smooth);
    EXPECT_EQ(rank.seriesStyle("rank"), (SeriesStyle{RgbaColor{1, 1, 1, 255}, 5.0}));
    EXPECT_EQ(playtime.appearance.connection, ConnectionMode::PointsOnly);
    EXPECT_EQ(playtime.seriesStyle("playtime").color, (RgbaColor{77, 208, 225, 255}));
}

TEST(BenchmarkTrackingStyle, RangeAndWindowChangesTakeTheirDataPathsAndNotTheStylePath) {
    StyleFixture fixture;
    fixture.startStyle();

    fixture.rankPreferences().range = RangePreference{RecentDays{2}, false};
    fixture.playtimePreferences().averagingWindowDays = 5;
    fixture.publishStyleEdits();

    EXPECT_EQ(fixture.style, 0);
    EXPECT_EQ(fixture.rankOnly, 1);
    EXPECT_EQ(fixture.playtimeOnly, 1);
}

TEST(BenchmarkTrackingStyle, TheStyleFollowsTheGraphAcrossBenchmarkSelection) {
    StyleFixture fixture;
    fixture.startStyle();
    fixture.rankPreferences().seriesStyles["rank"] = {RgbaColor{4, 4, 4, 255}, std::nullopt};
    fixture.publishStyleEdits();
    const auto committed = fixture.tracking->presentation(GraphKey::BenchmarkRank);

    fixture.tracking->clearSelection();
    fixture.tracking->select(BenchmarkId{"b1"});

    EXPECT_EQ(fixture.tracking->presentation(GraphKey::BenchmarkRank), committed);
    EXPECT_EQ(committed.seriesStyle("rank").color, (RgbaColor{4, 4, 4, 255}));
}

TEST(BenchmarkTrackingStyle, OtherGraphsStyleChangesAreIgnored) {
    StyleFixture fixture;
    fixture.startStyle();

    fixture.configuration->configuration.preferences(GraphKey::ScenarioHistory).appearance.connection =
        ConnectionMode::Straight;
    fixture.configuration->configuration.preferences(GraphKey::DailyPlaytime).appearance.connection =
        ConnectionMode::Straight;
    fixture.publishStyleEdits();

    EXPECT_EQ(fixture.style, 0);
}
