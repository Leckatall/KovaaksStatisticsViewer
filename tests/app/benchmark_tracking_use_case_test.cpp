#include <gtest/gtest.h>

#include <memory>

#include "benchmark_builders.h"
#include "counting_ids.h"
#include "fake_benchmark_repository.h"
#include "fake_playlist_reader.h"
#include "fake_profile_service.h"
#include "usecases/benchmark_library_service.h"
#include "usecases/benchmark_tracking_use_case.h"

using namespace ksv;
using namespace ksv::application;
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
        std::shared_ptr<FakeBenchmarkRepository> repo = std::make_shared<FakeBenchmarkRepository>();
        std::shared_ptr<FakePlaylistReader> reader = std::make_shared<FakePlaylistReader>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<BenchmarkLibraryService> library;
        std::unique_ptr<BenchmarkTrackingUseCase> tracking;

        void known(const std::string &name, const std::string &hash) {
            profile->scenarios.push_back({name, hash});
        }

        void build() {
            library = std::make_shared<BenchmarkLibraryService>(repo, reader, profile, countingIds());
            tracking = std::make_unique<BenchmarkTrackingUseCase>(library, profile);
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

    fixture.library->refresh();
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
    fixture.library->refresh();
    EXPECT_GT(fixture.profile->fact_request_count, changed);
    fixture.repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
    fixture.library->refresh();
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
    fixture.library->refresh();

    ASSERT_NE(fixture.tracking->projection(), nullptr);
    ASSERT_TRUE(fixture.tracking->projection()->averageRank);
    EXPECT_DOUBLE_EQ(*fixture.tracking->projection()->averageRank, 3.0);
    ASSERT_EQ(fixture.tracking->projection()->averageRankHistory.size(), 2U);
    EXPECT_DOUBLE_EQ(fixture.tracking->projection()->averageRankHistory.back().averageRank, 3.0);
}
