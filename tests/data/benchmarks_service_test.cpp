#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_validation.h"
#include "data/benchmarks_service.h"
#include "fake_benchmark_store.h"

using namespace ksv;
using namespace ksv::data;
using ksv::tests_support::FakeBenchmarkStore;

namespace {
    domain::Benchmark bench(const std::string &id, const std::string &name) {
        domain::Benchmark b;
        b.id = domain::BenchmarkId{id};
        b.name = name;
        return b;
    }

    BenchmarkFileEntry loadedEntry(std::string filename, std::string digest,
                                   const std::string &id, const std::string &name) {
        auto b = bench(id, name);
        auto completeness = domain::validateBenchmark(b);
        return BenchmarkFileEntry{std::move(filename), std::move(digest),
                                  LoadedBenchmark{std::move(b), completeness}};
    }

    BenchmarkFileEntry problemEntry(std::string filename, std::string digest) {
        return BenchmarkFileEntry{std::move(filename), std::move(digest),
                                  ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}}};
    }

    BenchmarkScanResult okScan(std::vector<BenchmarkFileEntry> entries) {
        return BenchmarkScanResult{BenchmarkLibrarySnapshot{std::move(entries)}, std::nullopt};
    }

    BenchmarkScanResult failedScan() {
        return BenchmarkScanResult{std::nullopt, BenchmarkScanFailure::DirectoryUnavailable};
    }

    std::vector<std::string> filenames(const BenchmarkLibrarySnapshot &s) {
        std::vector<std::string> out;
        for (const auto &e: s.entries) out.push_back(e.filename);
        return out;
    }

    std::vector<std::string> digests(const BenchmarkLibrarySnapshot &s) {
        std::vector<std::string> out;
        for (const auto &e: s.entries) out.push_back(e.digest);
        return out;
    }

    const LoadedBenchmark *loadedFor(const BenchmarkLibrarySnapshot &s, const std::string &filename) {
        for (const auto &e: s.entries)
            if (e.filename == filename) return std::get_if<LoadedBenchmark>(&e.content);
        return nullptr;
    }
}

TEST(BenchmarksService, ConstructionAcceptsStartupSnapshotAndPublishesRevision) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->directory = "/managed/benchmarks";
    store->nextScan = okScan({loadedEntry("a.json", "da", "b1", "Alpha"),
                              problemEntry("z.json", "dz")});

    BenchmarksService service(store);

    ASSERT_TRUE(service.snapshot().has_value());
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"a.json", "z.json"}));
    EXPECT_EQ(digests(*service.snapshot()), (std::vector<std::string>{"da", "dz"}));
    EXPECT_EQ(service.revision(), 1U);
    EXPECT_FALSE(service.lastRefreshFailed());
    EXPECT_EQ(service.managedDirectoryPath(), "/managed/benchmarks");
}

TEST(BenchmarksService, ConstructionFailurePublishesDiagnosticWithoutAcceptedSnapshot) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = failedScan();

    BenchmarksService service(store);

    EXPECT_FALSE(service.snapshot().has_value());
    EXPECT_EQ(service.revision(), 0U);
    EXPECT_TRUE(service.lastRefreshFailed());
}

TEST(BenchmarksService, RefreshReplacesTheCompleteAcceptedSnapshotOnce) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->scriptedScans = {
        okScan({loadedEntry("a.json", "da", "b1", "Alpha"),
                loadedEntry("b.json", "db", "b2", "Beta")}),
        okScan({loadedEntry("a.json", "da2", "b1", "Alpha renamed"),  // modification
                loadedEntry("c.json", "dc", "b3", "Gamma")}),          // b.json removed, c.json added
    };

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    service.refresh();

    ASSERT_TRUE(service.snapshot().has_value());
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"a.json", "c.json"}));
    EXPECT_EQ(digests(*service.snapshot()), (std::vector<std::string>{"da2", "dc"}));
    EXPECT_EQ(service.revision(), revisionBefore + 1);
    EXPECT_FALSE(service.lastRefreshFailed());
    EXPECT_EQ(callbacks, 1);
}

TEST(BenchmarksService, DirectoryFailurePreservesAcceptedSnapshotAndReportsFailure) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->scriptedScans = {okScan({loadedEntry("a.json", "da", "b1", "Alpha")})};
    store->nextScan = failedScan();

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    service.refresh();

    ASSERT_TRUE(service.snapshot().has_value());
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"a.json"}));
    EXPECT_EQ(service.revision(), revisionBefore);
    EXPECT_TRUE(service.lastRefreshFailed());
    EXPECT_EQ(callbacks, 1);
}

TEST(BenchmarksService, SaveNewIncompleteBenchmarkReturnsAdmittedTokenAndPublishes) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({});
    store->nextWrite = BenchmarkWriteResult{std::string{"newdigest"}, std::nullopt};

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    auto incomplete = bench("b9", "Fresh");  // name only: no tiers, no scenarios
    const auto outcome = service.save(BenchmarkSaveRequest{incomplete, std::nullopt});

    ASSERT_TRUE(outcome.ok());
    ASSERT_TRUE(outcome.token.has_value());
    EXPECT_EQ(outcome.token->id.value, "b9");
    EXPECT_EQ(outcome.token->filename, "b9.json");
    EXPECT_EQ(outcome.token->digest, "newdigest");

    EXPECT_EQ(store->lastWriteFilename, "b9.json");
    EXPECT_FALSE(store->lastWriteDigest.has_value());

    ASSERT_TRUE(service.snapshot().has_value());
    const auto *entry = loadedFor(*service.snapshot(), "b9.json");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->benchmark.name, "Fresh");
    EXPECT_EQ(entry->completeness.completeness, domain::Completeness::Incomplete);
    EXPECT_EQ(service.revision(), revisionBefore + 1);
    EXPECT_EQ(callbacks, 1);
}

TEST(BenchmarksService, SaveReplacementRequiresMatchingAcceptedToken) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("b1.json", "d1", "b1", "Alpha")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    auto edited = bench("b1", "Alpha edited");
    const BenchmarkEditToken staleToken{domain::BenchmarkId{"b1"}, "b1.json", "WRONG-DIGEST"};
    const auto outcome = service.save(BenchmarkSaveRequest{edited, staleToken});

    EXPECT_FALSE(outcome.ok());
    EXPECT_TRUE(outcome.error.has_value());
    EXPECT_EQ(store->writeCount, 0);
    ASSERT_TRUE(service.snapshot().has_value());
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"b1.json"}));
    EXPECT_EQ(digests(*service.snapshot()), (std::vector<std::string>{"d1"}));
    EXPECT_EQ(service.revision(), revisionBefore);
    EXPECT_EQ(callbacks, 0);
}

TEST(BenchmarksService, SaveConflictOrWriteFailurePreservesAcceptedState) {
    for (const auto failure: {BenchmarkWriteFailure::ExternalModificationConflict,
                              BenchmarkWriteFailure::WriteFailed}) {
        auto store = std::make_shared<FakeBenchmarkStore>();
        store->nextScan = okScan({loadedEntry("b1.json", "d1", "b1", "Alpha")});

        BenchmarksService service(store);
        int callbacks = 0;
        service.onChanged([&] { ++callbacks; });
        const auto revisionBefore = service.revision();

        store->nextWrite = BenchmarkWriteResult{std::nullopt, failure};
        auto edited = bench("b1", "Alpha edited");
        const BenchmarkEditToken token{domain::BenchmarkId{"b1"}, "b1.json", "d1"};
        const auto outcome = service.save(BenchmarkSaveRequest{edited, token});

        EXPECT_FALSE(outcome.ok());
        ASSERT_TRUE(outcome.error.has_value());
        EXPECT_EQ(*outcome.error,
                  failure == BenchmarkWriteFailure::ExternalModificationConflict
                      ? BenchmarkSaveError::Conflict
                      : BenchmarkSaveError::WriteFailed);
        ASSERT_TRUE(service.snapshot().has_value());
        const auto *entry = loadedFor(*service.snapshot(), "b1.json");
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(entry->benchmark.name, "Alpha");
        EXPECT_EQ(service.snapshot()->entries.at(0).digest, "d1");
        EXPECT_EQ(service.revision(), revisionBefore);
        EXPECT_EQ(callbacks, 0);
    }
}

TEST(BenchmarksService, DeleteMatchingTokenRemovesEntryAndPublishes) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("b1.json", "d1", "b1", "Alpha"),
                              loadedEntry("b2.json", "d2", "b2", "Beta")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    store->nextRemove = BenchmarkDeleteResult{true, std::nullopt};
    const BenchmarkEditToken token{domain::BenchmarkId{"b1"}, "b1.json", "d1"};
    const auto outcome = service.remove(token);

    EXPECT_TRUE(outcome.ok());
    EXPECT_EQ(store->removeCount, 1);
    EXPECT_EQ(store->lastRemoveFilename, "b1.json");
    EXPECT_EQ(store->lastRemoveDigest, "d1");
    ASSERT_TRUE(service.snapshot().has_value());
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"b2.json"}));
    EXPECT_EQ(service.revision(), revisionBefore + 1);
    EXPECT_EQ(callbacks, 1);
}

TEST(BenchmarksService, DeleteUnknownStaleOrFailedTokenPreservesAcceptedState) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("b1.json", "d1", "b1", "Alpha")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    const auto expectPreserved = [&] {
        ASSERT_TRUE(service.snapshot().has_value());
        EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"b1.json"}));
        EXPECT_EQ(service.revision(), revisionBefore);
        EXPECT_EQ(callbacks, 0);
    };

    // Unknown filename: never reaches the store.
    auto unknown = service.remove(BenchmarkEditToken{domain::BenchmarkId{"bX"}, "bX.json", "dX"});
    EXPECT_FALSE(unknown.ok());
    EXPECT_EQ(store->removeCount, 0);
    expectPreserved();

    // Stale digest for a known file: never reaches the store.
    auto stale = service.remove(BenchmarkEditToken{domain::BenchmarkId{"b1"}, "b1.json", "STALE"});
    EXPECT_FALSE(stale.ok());
    EXPECT_EQ(store->removeCount, 0);
    expectPreserved();

    // Matching token, but the store declines: classified outcome, accepted state intact.
    for (const auto failure: {BenchmarkWriteFailure::ExternalModificationConflict,
                              BenchmarkWriteFailure::WriteFailed}) {
        store->nextRemove = BenchmarkDeleteResult{false, failure};
        auto declined = service.remove(BenchmarkEditToken{domain::BenchmarkId{"b1"}, "b1.json", "d1"});
        EXPECT_FALSE(declined.ok());
        ASSERT_TRUE(declined.error.has_value());
        EXPECT_EQ(*declined.error,
                  failure == BenchmarkWriteFailure::ExternalModificationConflict
                      ? BenchmarkRemoveError::Conflict
                      : BenchmarkRemoveError::WriteFailed);
        expectPreserved();
    }
}

TEST(BenchmarksService, BatchInstallsExactlySuccessfulReplacementsAndReportsEveryOutcome) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("a.json", "da", "ba", "Aaa"),
                              loadedEntry("b.json", "db", "bb", "Bbb"),
                              loadedEntry("c.json", "dc", "bc", "Ccc")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    store->scriptedWrites = {
        BenchmarkWriteResult{std::string{"da2"}, std::nullopt},
        BenchmarkWriteResult{std::nullopt, BenchmarkWriteFailure::ExternalModificationConflict},
        BenchmarkWriteResult{std::nullopt, BenchmarkWriteFailure::WriteFailed},
    };
    const std::vector<BenchmarkReplacementRequest> requests{
        {bench("ba", "Aaa mapped"), {domain::BenchmarkId{"ba"}, "a.json", "da"}},
        {bench("bb", "Bbb mapped"), {domain::BenchmarkId{"bb"}, "b.json", "db"}},
        {bench("bc", "Ccc mapped"), {domain::BenchmarkId{"bc"}, "c.json", "dc"}},
    };

    const auto outcome = service.replaceBatch(requests);

    ASSERT_EQ(outcome.outcomes.size(), 3U);
    EXPECT_TRUE(outcome.outcomes[0].ok());
    ASSERT_TRUE(outcome.outcomes[0].token.has_value());
    EXPECT_EQ(outcome.outcomes[0].token->digest, "da2");
    EXPECT_FALSE(outcome.outcomes[1].ok());
    EXPECT_EQ(outcome.outcomes[1].error, BenchmarkSaveError::Conflict);
    EXPECT_FALSE(outcome.outcomes[2].ok());
    EXPECT_EQ(outcome.outcomes[2].error, BenchmarkSaveError::WriteFailed);

    ASSERT_TRUE(service.snapshot().has_value());
    const auto *a = loadedFor(*service.snapshot(), "a.json");
    const auto *b = loadedFor(*service.snapshot(), "b.json");
    const auto *c = loadedFor(*service.snapshot(), "c.json");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(a->benchmark.name, "Aaa mapped");
    EXPECT_EQ(service.snapshot()->entries.at(0).digest, "da2");
    EXPECT_EQ(b->benchmark.name, "Bbb");   // conflict: untouched
    EXPECT_EQ(service.snapshot()->entries.at(1).digest, "db");
    EXPECT_EQ(c->benchmark.name, "Ccc");   // write failure: untouched
    EXPECT_EQ(service.snapshot()->entries.at(2).digest, "dc");

    EXPECT_EQ(service.revision(), revisionBefore + 1);
    EXPECT_EQ(callbacks, 1);
}

TEST(BenchmarksService, BatchPublishesAtMostOnceAfterAllWrites) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("a.json", "da", "ba", "Aaa"),
                              loadedEntry("b.json", "db", "bb", "Bbb")});

    BenchmarksService service(store);
    int callbacks = 0;
    std::vector<std::string> observedNamesAtCallback;
    service.onChanged([&] {
        ++callbacks;
        observedNamesAtCallback.clear();
        const auto snap = service.snapshot();
        if (snap)
            for (const auto &e: snap->entries)
                if (const auto *l = std::get_if<LoadedBenchmark>(&e.content))
                    observedNamesAtCallback.push_back(l->benchmark.name);
    });
    const auto revisionBefore = service.revision();

    store->scriptedWrites = {
        BenchmarkWriteResult{std::string{"da2"}, std::nullopt},
        BenchmarkWriteResult{std::string{"db2"}, std::nullopt},
    };
    const std::vector<BenchmarkReplacementRequest> requests{
        {bench("ba", "Aaa2"), {domain::BenchmarkId{"ba"}, "a.json", "da"}},
        {bench("bb", "Bbb2"), {domain::BenchmarkId{"bb"}, "b.json", "db"}},
    };

    service.replaceBatch(requests);

    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(observedNamesAtCallback, (std::vector<std::string>{"Aaa2", "Bbb2"}));
    EXPECT_EQ(service.revision(), revisionBefore + 1);
}

TEST(BenchmarksService, BatchWithNoSuccessDoesNotAdvanceAcceptedRevision) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("a.json", "da", "ba", "Aaa")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    store->scriptedWrites = {BenchmarkWriteResult{std::nullopt, BenchmarkWriteFailure::WriteFailed}};
    const std::vector<BenchmarkReplacementRequest> requests{
        {bench("ba", "Aaa2"), {domain::BenchmarkId{"ba"}, "a.json", "da"}},          // store fails
        {bench("bZ", "Zzz"), {domain::BenchmarkId{"bZ"}, "z.json", "dz"}},           // unknown token
    };

    const auto outcome = service.replaceBatch(requests);

    ASSERT_EQ(outcome.outcomes.size(), 2U);
    EXPECT_FALSE(outcome.outcomes[0].ok());
    EXPECT_FALSE(outcome.outcomes[1].ok());
    ASSERT_TRUE(service.snapshot().has_value());
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"a.json"}));
    EXPECT_EQ(service.snapshot()->entries.at(0).digest, "da");
    EXPECT_EQ(service.revision(), revisionBefore);
    EXPECT_EQ(callbacks, 0);
}
