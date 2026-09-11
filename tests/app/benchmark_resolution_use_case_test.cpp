#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "benchmark_builders.h"
#include "fake_benchmarks_service.h"
#include "fake_profile_service.h"
#include "usecases/benchmark_resolution_use_case.h"

using namespace ksv;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    Benchmark benchmarkWith(const std::string &id, std::vector<ScenarioEntry> entries) {
        Benchmark value;
        value.id = BenchmarkId{id};
        value.name = "Bench " + id;
        value.uncategorized = std::move(entries);
        return value;
    }

    data::BenchmarkFileEntry loadedFile(const Benchmark &value) {
        return {value.id.value + ".json", "d-" + value.id.value,
                data::LoadedBenchmark{value, validateBenchmark(value)}};
    }

    struct Fixture {
        std::shared_ptr<FakeBenchmarksService> benchmarks = std::make_shared<FakeBenchmarksService>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();

        void played(const std::string &name, const std::string &hash, int count = 1) {
            const ScenarioId scenario{name, hash};
            profile->scenarios.push_back(scenario);
            profile->run_counts[scenario] = static_cast<std::size_t>(count);
        }

        void library(const std::vector<Benchmark> &benches) {
            data::BenchmarkLibrarySnapshot snapshot;
            for (const auto &value: benches) snapshot.entries.push_back(loadedFile(value));
            benchmarks->snapshotValue = snapshot;
        }

        std::unique_ptr<application::BenchmarkResolutionUseCase> build() {
            return std::make_unique<application::BenchmarkResolutionUseCase>(benchmarks, profile);
        }
    };

    const std::vector<ScenarioResolution> &resolutionsFor(
        const application::BenchmarkResolutionSnapshot &snapshot, const std::string &id) {
        return snapshot.resolutions.at(BenchmarkId{id});
    }

    const ScenarioResolution &forEntry(const std::vector<ScenarioResolution> &resolutions, const char *id) {
        return *std::ranges::find_if(resolutions, [id](const ScenarioResolution &resolution) {
            return resolution.entryId.value == id;
        });
    }

    data::BenchmarkSaveOutcome admitted(const std::string &benchmarkId) {
        return {data::BenchmarkEditToken{BenchmarkId{benchmarkId}, benchmarkId + ".json",
                                         "d2-" + benchmarkId},
                std::nullopt};
    }
}

TEST(BenchmarkResolutionUseCase, DerivesMappedUnavailableUnresolvedAmbiguousAndUniqueStates) {
    Fixture f;
    f.played("Many", "hMB", 7);
    f.played("Many", "hMA", 2);
    f.played("Unique", "hU", 4);
    f.played("Resolved", "hR", 3);
    f.played("Pinned", "hP", 5);
    f.played("Pinned", "hPx", 1);
    f.library({benchmarkWith("b1", {
        benchmarkEntry("res", "Resolved", std::string{"hR"}, {}),
        benchmarkEntry("gone", "Gone", std::string{"hMiss"}, {}),
        benchmarkEntry("none", "None", std::nullopt, {}),
        benchmarkEntry("many", "Many", std::nullopt, {}),
        benchmarkEntry("uni", "Unique", std::nullopt, {}),
        benchmarkEntry("pin", "Pinned", std::string{"hP"}, {}),
    })});

    const auto useCase = f.build();
    const auto snap = useCase->snapshot();

    ASSERT_TRUE(snap.resolutions.contains(BenchmarkId{"b1"}));
    const auto &rs = resolutionsFor(snap, "b1");
    ASSERT_EQ(rs.size(), 6U);
    EXPECT_EQ(forEntry(rs, "res").state, ScenarioMatchState::Resolved);
    EXPECT_EQ(forEntry(rs, "gone").state, ScenarioMatchState::MappedUnavailable);
    EXPECT_EQ(forEntry(rs, "none").state, ScenarioMatchState::Unresolved);
    EXPECT_EQ(forEntry(rs, "many").state, ScenarioMatchState::Ambiguous);
    EXPECT_EQ(forEntry(rs, "uni").state, ScenarioMatchState::AutoMappable);
    // A persisted hash is never re-decided by name, even with two same-name candidates.
    EXPECT_EQ(forEntry(rs, "pin").state, ScenarioMatchState::Resolved);
    EXPECT_EQ(forEntry(rs, "pin").hash, std::optional<std::string>{"hP"});

    ASSERT_EQ(forEntry(rs, "many").candidates.size(), 2U);
    EXPECT_EQ(forEntry(rs, "many").candidates[0].hash, "hMA");
    EXPECT_EQ(forEntry(rs, "many").candidates[1].hash, "hMB");
    ASSERT_EQ(forEntry(rs, "uni").candidates.size(), 1U);
    EXPECT_EQ(forEntry(rs, "uni").candidates[0].hash, "hU");

    ASSERT_EQ(snap.scenarioCatalogue.size(), 6U);
    EXPECT_EQ(snap.scenarioCatalogue.front().name, "Many");
    EXPECT_EQ(snap.scenarioCatalogue.front().hash, "hMA");
    EXPECT_EQ(snap.scenarioCatalogue[2].name, "Pinned");
    EXPECT_EQ(snap.scenarioCatalogue[2].hash, "hP");
    EXPECT_EQ(snap.scenarioCatalogue.back().name, "Unique");
    EXPECT_EQ(snap.scenarioCatalogue.back().hash, "hU");
}

TEST(BenchmarkResolutionUseCase, UniqueMappingsSubmitOneCompleteReplacementBatch) {
    Fixture f;
    f.played("Alpha", "hA");
    f.played("Beta", "hB");
    f.library({benchmarkWith("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, {})}),
               benchmarkWith("b2", {benchmarkEntry("e2", "Beta", std::nullopt, {})})});
    f.benchmarks->nextBatchOutcome = {{admitted("b1"), admitted("b2")}};

    const auto useCase = f.build();

    EXPECT_TRUE(f.benchmarks->saveRequests.empty());
    ASSERT_EQ(f.benchmarks->batchRequests.size(), 1U);
    const auto &batch = f.benchmarks->batchRequests.front();
    ASSERT_EQ(batch.size(), 2U);

    const auto *first = &batch[0];
    const auto *second = &batch[1];
    if (first->token.id.value == "b2") std::swap(first, second);

    EXPECT_EQ(first->token.id.value, "b1");
    EXPECT_EQ(first->token.filename, "b1.json");
    EXPECT_EQ(first->token.digest, "d-b1");
    ASSERT_EQ(first->benchmark.uncategorized.size(), 1U);
    EXPECT_EQ(first->benchmark.uncategorized[0].hash, std::optional<std::string>{"hA"});

    EXPECT_EQ(second->token.id.value, "b2");
    EXPECT_EQ(second->token.filename, "b2.json");
    EXPECT_EQ(second->token.digest, "d-b2");
    EXPECT_EQ(second->benchmark.uncategorized[0].hash, std::optional<std::string>{"hB"});
}

TEST(BenchmarkResolutionUseCase, AutoMappingNeverDuplicatesAnAlreadyUsedHash) {
    Fixture f;
    f.played("Alpha", "h1");
    f.library({benchmarkWith("b1", {benchmarkEntry("pinned", "Alpha", std::string{"h1"}, {}),
                                    benchmarkEntry("dupe", "Alpha", std::nullopt, {})})});
    f.benchmarks->nextBatchOutcome = {{admitted("b1")}};

    const auto useCase = f.build();
    const auto snap = useCase->snapshot();

    ASSERT_TRUE(snap.resolutions.contains(BenchmarkId{"b1"}));
    EXPECT_EQ(forEntry(resolutionsFor(snap, "b1"), "dupe").state, ScenarioMatchState::AutoMappable);

    for (const auto &batch: f.benchmarks->batchRequests)
        for (const auto &request: batch)
            for (const auto &entry: request.benchmark.uncategorized)
                EXPECT_FALSE(entry.id.value == "dupe" && entry.hash == std::optional<std::string>{"h1"});
}

TEST(BenchmarkResolutionUseCase, ProfileOnlyChangePublishesDerivedStateWithoutWrite) {
    Fixture f;
    f.library({benchmarkWith("b1", {benchmarkEntry("e1", "Alpha", std::string{"h1"}, {})})});

    const auto useCase = f.build();
    ASSERT_TRUE(useCase->snapshot().resolutions.contains(BenchmarkId{"b1"}));
    ASSERT_EQ(forEntry(resolutionsFor(useCase->snapshot(), "b1"), "e1").state,
              ScenarioMatchState::MappedUnavailable);

    int published = 0;
    useCase->onChanged([&] { ++published; });
    f.played("Alpha", "h1");
    f.profile->notifyProfileChanged();

    const auto snap = useCase->snapshot();
    EXPECT_EQ(published, 1);
    EXPECT_EQ(forEntry(resolutionsFor(snap, "b1"), "e1").state, ScenarioMatchState::Resolved);
    EXPECT_EQ(snap.scenarioCatalogue.size(), 1U);
    EXPECT_TRUE(f.benchmarks->batchRequests.empty());
}

TEST(BenchmarkResolutionUseCase, BatchFailuresPublishDiagnosticsWithoutInventingAcceptedMappings) {
    Fixture f;
    f.played("Alpha", "h1");
    f.library({benchmarkWith("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, {})})});
    f.benchmarks->nextBatchOutcome = {{data::BenchmarkSaveOutcome{
        std::nullopt, data::BenchmarkSaveError::Conflict}}};

    const auto useCase = f.build();
    const auto snap = useCase->snapshot();

    ASSERT_TRUE(snap.resolutions.contains(BenchmarkId{"b1"}));
    EXPECT_EQ(forEntry(resolutionsFor(snap, "b1"), "e1").state, ScenarioMatchState::AutoMappable);
    EXPECT_NE(forEntry(resolutionsFor(snap, "b1"), "e1").state, ScenarioMatchState::Resolved);

    EXPECT_TRUE(snap.anyAutomaticWriteFailed());
    ASSERT_TRUE(snap.automaticWriteFailures.contains(BenchmarkId{"b1"}));
    EXPECT_EQ(snap.automaticWriteFailures.at(BenchmarkId{"b1"}),
              application::AutomaticMappingWriteError::Conflict);
}

TEST(BenchmarkResolutionUseCase, ServiceCallbackReentrySettlesAfterSuccessfulMapping) {
    Fixture f;
    f.played("Alpha", "h1");
    f.library({benchmarkWith("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, {})})});
    f.benchmarks->nextBatchOutcome = {{admitted("b1")}};
    f.benchmarks->republishReplacementsSynchronously = true;

    const auto useCase = f.build();
    int published = 0;
    useCase->onChanged([&] { ++published; });
    f.profile->notifyProfileChanged();

    const auto snap = useCase->snapshot();
    ASSERT_EQ(f.benchmarks->batchRequests.size(), 1U);
    ASSERT_TRUE(snap.resolutions.contains(BenchmarkId{"b1"}));
    EXPECT_EQ(forEntry(resolutionsFor(snap, "b1"), "e1").state, ScenarioMatchState::Resolved);
    EXPECT_EQ(published, 1);
}

TEST(BenchmarkResolutionUseCase, EditLeaseDefersOnlyTheLeasedBenchmarkWrite) {
    Fixture f;
    f.played("Alpha", "hA");
    f.played("Beta", "hB");
    f.library({benchmarkWith("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, {})}),
               benchmarkWith("b2", {benchmarkEntry("e2", "Beta", std::nullopt, {})})});
    f.benchmarks->nextBatchOutcome = {{admitted("b1"), admitted("b2")}};

    const auto useCase = f.build();
    useCase->setEditLease(BenchmarkId{"b1"});
    f.benchmarks->batchRequests.clear();
    f.profile->notifyProfileChanged();

    ASSERT_EQ(f.benchmarks->batchRequests.size(), 1U);
    const auto &batch = f.benchmarks->batchRequests.front();
    ASSERT_EQ(batch.size(), 1U);
    EXPECT_EQ(batch[0].token.id.value, "b2");

    const auto snap = useCase->snapshot();
    EXPECT_EQ(forEntry(resolutionsFor(snap, "b1"), "e1").state, ScenarioMatchState::AutoMappable);
    EXPECT_EQ(forEntry(resolutionsFor(snap, "b2"), "e2").state, ScenarioMatchState::AutoMappable);
}

TEST(BenchmarkResolutionUseCase, ReleasingLeaseReconcilesLatestAcceptedBenchmark) {
    Fixture f;
    f.library({benchmarkWith("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, {})})});
    f.benchmarks->nextBatchOutcome = {{admitted("b1")}};

    const auto useCase = f.build();
    useCase->setEditLease(BenchmarkId{"b1"});

    // Explicit refresh replaces b1's accepted definition and digest while the lease is held.
    data::BenchmarkLibrarySnapshot refreshed;
    Benchmark v2 = benchmarkWith("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, {})});
    refreshed.entries.push_back({"b1.json", "d-b1-v2",
                                 data::LoadedBenchmark{v2, validateBenchmark(v2)}});
    f.benchmarks->snapshotValue = refreshed;
    f.played("Alpha", "hA");           // e1 is now uniquely resolvable
    f.benchmarks->fireChanged();

    const auto batchesBeforeRelease = f.benchmarks->batchRequests.size();
    useCase->setEditLease(std::nullopt);

    ASSERT_GT(f.benchmarks->batchRequests.size(), batchesBeforeRelease);
    const auto &released = f.benchmarks->batchRequests.back();
    ASSERT_EQ(released.size(), 1U);
    EXPECT_EQ(released[0].token.id.value, "b1");
    EXPECT_EQ(released[0].token.filename, "b1.json");
    EXPECT_EQ(released[0].token.digest, "d-b1-v2");  // latest, never the pre-refresh "d-b1"
    ASSERT_EQ(released[0].benchmark.uncategorized.size(), 1U);
    EXPECT_EQ(released[0].benchmark.uncategorized[0].hash, std::optional<std::string>{"hA"});

    for (const auto &batch: f.benchmarks->batchRequests)
        for (const auto &request: batch)
            EXPECT_NE(request.token.digest, "d-b1");
}

TEST(BenchmarkResolutionUseCase, NewDraftLeaseDoesNotSuppressAcceptedBenchmarks) {
    Fixture f;
    f.played("Alpha", "hA");
    f.library({benchmarkWith("b1", {benchmarkEntry("e1", "Alpha", std::nullopt, {})})});
    f.benchmarks->nextBatchOutcome = {{admitted("b1")}};

    const auto useCase = f.build();
    useCase->setEditLease(BenchmarkId{"draft-new"});  // reserved id of an unsaved working copy
    f.benchmarks->batchRequests.clear();
    f.profile->notifyProfileChanged();

    ASSERT_EQ(f.benchmarks->batchRequests.size(), 1U);
    const auto &batch = f.benchmarks->batchRequests.front();
    ASSERT_EQ(batch.size(), 1U);
    EXPECT_EQ(batch[0].token.id.value, "b1");

    EXPECT_FALSE(useCase->snapshot().resolutions.contains(BenchmarkId{"draft-new"}));
}
