#include <gtest/gtest.h>

#include "usecases/benchmark_library_service.h"
#include "fake_benchmark_repository.h"

using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    BenchmarkLibrarySnapshot snapshotWith(const char *filename) {
        BenchmarkLibrarySnapshot snapshot;
        snapshot.entries.push_back({filename, "digest",
                                    ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}}});
        return snapshot;
    }
}

TEST(BenchmarkLibraryService, ConstructionLoadsTheStartupSnapshotAndBumpsRevision) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshotWith("a.json"), std::nullopt};
    BenchmarkLibraryService service(repo);
    EXPECT_EQ(repo->scanCount, 1);
    ASSERT_TRUE(service.snapshot().has_value());
    EXPECT_EQ(service.snapshot()->entries.size(), 1U);
    EXPECT_EQ(service.revision(), 1U);
    EXPECT_FALSE(service.lastRefreshFailed());
}

TEST(BenchmarkLibraryService, RefreshPublishesAndAdvancesRevision) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshotWith("a.json"), std::nullopt};
    BenchmarkLibraryService service(repo);

    int published = 0;
    service.onChanged([&] { ++published; });
    repo->nextScan = {snapshotWith("b.json"), std::nullopt};
    service.refresh();

    EXPECT_EQ(published, 1);
    EXPECT_EQ(service.revision(), 2U);
    EXPECT_EQ(service.snapshot()->entries.at(0).filename, "b.json");
}

TEST(BenchmarkLibraryService, DirectoryFailurePreservesLastSnapshotAndReportsError) {
    auto repo = std::make_shared<FakeBenchmarkRepository>();
    repo->nextScan = {snapshotWith("a.json"), std::nullopt};
    BenchmarkLibraryService service(repo);
    const auto revisionBefore = service.revision();

    int published = 0;
    service.onChanged([&] { ++published; });
    repo->nextScan = {std::nullopt, BenchmarkScanFailure::DirectoryUnavailable};
    service.refresh();

    EXPECT_TRUE(service.lastRefreshFailed());
    EXPECT_EQ(service.revision(), revisionBefore);  // not advanced
    ASSERT_TRUE(service.snapshot().has_value());
    EXPECT_EQ(service.snapshot()->entries.at(0).filename, "a.json");  // preserved
    EXPECT_EQ(published, 1);  // failure is still published so the UI can show it
}
