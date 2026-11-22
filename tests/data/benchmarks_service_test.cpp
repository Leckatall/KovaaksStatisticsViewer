#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "benchmarks/benchmark_definition.h"
#include "benchmarks/benchmark_validation.h"
#include "data/benchmarks_service.h"
#include "fake_benchmark_store.h"

using namespace ksv;
using namespace ksv::data;
using ksv::tests_support::FakeBenchmarkStore;

namespace {
    // Benchmarks are told apart by content; the store never supplies an id or a name.
    domain::BenchmarkDefinition bench(const std::string &marker) {
        domain::BenchmarkDefinition b;
        b.tiers = {domain::Tier{domain::TierId{"t1"}, marker, {}}};
        return b;
    }

    // A working copy as the editor submits it: its own key and the name it should be saved under.
    domain::BenchmarkDefinition draft(const domain::BenchmarkId &id, const std::string &name,
                                      const std::string &marker) {
        auto b = bench(marker);
        b.id = id;
        b.name = name;
        return b;
    }

    std::string markerOf(const domain::BenchmarkDefinition &b) { return b.tiers.empty() ? "" : b.tiers[0].name; }

    BenchmarkFileEntry loadedEntry(std::string filename, std::string digest, const std::string &marker = "") {
        auto b = bench(marker);
        auto completeness = domain::validateBenchmark(b);
        return BenchmarkFileEntry{std::move(filename), std::move(digest), LoadedBenchmark{std::move(b), completeness}};
    }

    BenchmarkFileEntry problemEntry(std::string filename, std::string digest,
                                    BenchmarkFileProblem problem = BenchmarkFileProblem::Invalid) {
        return BenchmarkFileEntry{std::move(filename), std::move(digest), ProblemBenchmark{problem, {}, {}}};
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

    const BenchmarkFileEntry *entryFor(const BenchmarkLibrarySnapshot &s, const std::string &filename) {
        for (const auto &e: s.entries)
            if (e.filename == filename) return &e;
        return nullptr;
    }

    const LoadedBenchmark *loadedFor(const BenchmarkLibrarySnapshot &s, const std::string &filename) {
        const auto *entry = entryFor(s, filename);
        return entry ? std::get_if<LoadedBenchmark>(&entry->content) : nullptr;
    }

    domain::BenchmarkId keyOf(const BenchmarksService &service, const std::string &filename) {
        const auto snapshot = service.snapshot();
        const auto *entry = entryFor(*snapshot, filename);
        EXPECT_NE(entry, nullptr) << filename;
        return entry ? entry->id : domain::BenchmarkId{};
    }

    // The token the manager would build when opening `filename`.
    BenchmarkEditToken tokenFor(const BenchmarksService &service, const std::string &filename) {
        const auto snapshot = service.snapshot();
        const auto *entry = entryFor(*snapshot, filename);
        EXPECT_NE(entry, nullptr) << filename;
        return entry ? BenchmarkEditToken{entry->id, entry->filename, entry->digest} : BenchmarkEditToken{};
    }
}

TEST(BenchmarksService, ConstructionAcceptsStartupSnapshotAndPublishesRevision) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->directory = "/managed/benchmarks";
    store->nextScan = okScan({loadedEntry("a.json", "da"), problemEntry("z.json", "dz")});

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

TEST(BenchmarksService, EveryEntryIsKeyedAndNamedAfterItsFilename) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("Alpha.json", "da"), loadedEntry("Beta.JSON", "db"),
                              problemEntry("Broken.json", "dc"),
                              problemEntry("Future.json", "dd", BenchmarkFileProblem::Unsupported),
                              problemEntry("Old.json", "de", BenchmarkFileProblem::MigrationFailed)});

    BenchmarksService service(store);

    const auto library = service.snapshot();
    const auto &entries = library->entries;
    std::vector<std::string> names;
    std::set<std::string> keys;
    for (const auto &entry: entries) {
        names.push_back(entry.name);
        keys.insert(entry.id.value);
        if (const auto *loaded = std::get_if<LoadedBenchmark>(&entry.content)) {
            EXPECT_EQ(loaded->benchmark.id, entry.id);
            EXPECT_EQ(loaded->benchmark.name, entry.name);
        }
    }
    EXPECT_EQ(names, (std::vector<std::string>{"Alpha", "Beta", "Broken", "Future", "Old"}));
    EXPECT_EQ(keys.size(), entries.size());
    EXPECT_FALSE(keys.contains(""));
    // The store's definitions carry no name; once named after its file, a benchmark validates as such.
    EXPECT_TRUE(std::ranges::none_of(loadedFor(*service.snapshot(), "Alpha.json")->completeness.issues,
                                     [](const auto &issue) { return issue.code == domain::BenchmarkIssueCode::MissingName; }));
}

TEST(BenchmarksService, TwoFilesWithIdenticalContentAreTwoBenchmarks) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("Copy.json", "same", "x"), loadedEntry("Original.json", "same", "x")});

    BenchmarksService service(store);

    EXPECT_NE(keyOf(service, "Copy.json"), keyOf(service, "Original.json"));
}

TEST(BenchmarksService, KeysSurviveRefreshesWhileTheFilenameStaysButNotItsAbsence) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->scriptedScans = {
        okScan({loadedEntry("a.json", "da"), loadedEntry("b.json", "db")}),
        okScan({loadedEntry("a.json", "da2"), loadedEntry("c.json", "db")}),  // b.json renamed to c.json
        okScan({loadedEntry("a.json", "da2")}),
        okScan({loadedEntry("a.json", "da2"), loadedEntry("c.json", "db")}),  // c.json is back
    };

    BenchmarksService service(store);
    const auto a = keyOf(service, "a.json");
    const auto b = keyOf(service, "b.json");

    service.refresh();
    EXPECT_EQ(keyOf(service, "a.json"), a);
    const auto c = keyOf(service, "c.json");
    EXPECT_NE(c, b);

    service.refresh();
    service.refresh();
    EXPECT_EQ(keyOf(service, "a.json"), a);
    EXPECT_NE(keyOf(service, "c.json"), c);
}

TEST(BenchmarksService, RefreshReplacesTheCompleteAcceptedSnapshotOnce) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->scriptedScans = {
        okScan({loadedEntry("a.json", "da"), loadedEntry("b.json", "db")}),
        okScan({loadedEntry("a.json", "da2"),  // modification
                loadedEntry("c.json", "dc")}), // b.json removed, c.json added
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
    store->scriptedScans = {okScan({loadedEntry("a.json", "da")})};
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

TEST(BenchmarksService, CreateWritesTheNamedFileAndAdmitsItUnderTheDraftsKey) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({});
    store->nextWrite = BenchmarkWriteResult{std::string{"newdigest"}, std::nullopt};

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    const domain::BenchmarkId draftKey{"draft-uuid"};
    const auto outcome = service.save(BenchmarkSaveRequest{draft(draftKey, "Fresh", "x"), std::nullopt});

    ASSERT_TRUE(outcome.ok());
    EXPECT_EQ(outcome.token->id, draftKey);
    EXPECT_EQ(outcome.token->filename, "Fresh.json");
    EXPECT_EQ(outcome.token->digest, "newdigest");
    EXPECT_EQ(outcome.name, "Fresh");

    EXPECT_EQ(store->lastWriteFilename, "Fresh.json");
    EXPECT_FALSE(store->lastWriteDigest.has_value());
    EXPECT_EQ(store->renameCount, 0);

    EXPECT_EQ(keyOf(service, "Fresh.json"), draftKey);
    const auto snapshotNow = service.snapshot();
    const auto *entry = loadedFor(*snapshotNow, "Fresh.json");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->benchmark.name, "Fresh");
    EXPECT_EQ(entry->completeness.completeness, domain::Completeness::Incomplete);
    EXPECT_EQ(service.revision(), revisionBefore + 1);
    EXPECT_EQ(callbacks, 1);
}

TEST(BenchmarksService, ARenamingSaveWritesInPlaceThenMovesTheFileAndItsKey) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("Old.json", "d1", "before")});
    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto token = tokenFor(service, "Old.json");

    store->nextWrite = BenchmarkWriteResult{std::string{"d2"}, std::nullopt};
    const auto outcome = service.save({draft(token.id, "New", "after"), token});

    ASSERT_TRUE(outcome.ok());
    EXPECT_EQ(store->lastWriteFilename, "Old.json");
    EXPECT_EQ(store->lastWriteDigest, std::make_optional<std::string>("d1"));
    EXPECT_EQ(store->lastRenameFrom, "Old.json");
    EXPECT_EQ(store->lastRenameTo, "New.json");
    EXPECT_EQ(store->lastRenameDigest, "d2");
    EXPECT_EQ(outcome.token->id, token.id);
    EXPECT_EQ(outcome.token->filename, "New.json");
    EXPECT_EQ(outcome.token->digest, "d2");
    EXPECT_EQ(outcome.name, "New");
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"New.json"}));
    EXPECT_EQ(keyOf(service, "New.json"), token.id);
    EXPECT_EQ(markerOf(loadedFor(*service.snapshot(), "New.json")->benchmark), "after");
    EXPECT_EQ(callbacks, 1);

    store->nextWrite = BenchmarkWriteResult{std::string{"d3"}, std::nullopt};
    ASSERT_TRUE(service.save({draft(token.id, "New", "again"), outcome.token}).ok());
    EXPECT_EQ(store->lastWriteFilename, "New.json");
    EXPECT_EQ(store->renameCount, 1);
}

TEST(BenchmarksService, AnUnchangedNameNeverRenamesEvenWithAnUpperCaseExtension) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("Same.JSON", "d1")});
    store->nextWrite = BenchmarkWriteResult{std::string{"d2"}, std::nullopt};
    BenchmarksService service(store);
    const auto token = tokenFor(service, "Same.JSON");

    const auto outcome = service.save({draft(token.id, "Same", "edited"), token});

    ASSERT_TRUE(outcome.ok());
    EXPECT_EQ(store->renameCount, 0);
    EXPECT_EQ(outcome.token->filename, "Same.JSON");
}

TEST(BenchmarksService, ALetterCaseChangeRenames) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("alpha.json", "d1")});
    store->nextWrite = BenchmarkWriteResult{std::string{"d2"}, std::nullopt};
    BenchmarksService service(store);
    const auto token = tokenFor(service, "alpha.json");

    const auto outcome = service.save({draft(token.id, "Alpha", ""), token});

    ASSERT_TRUE(outcome.ok());
    EXPECT_EQ(store->lastRenameTo, "Alpha.json");
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"Alpha.json"}));
}

TEST(BenchmarksService, AFailedRenameKeepsTheNewContentUnderTheOldNameAndCanBeRetried) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("Old.json", "d1", "before")});
    BenchmarksService service(store);
    const auto token = tokenFor(service, "Old.json");

    store->nextWrite = BenchmarkWriteResult{std::string{"d2"}, std::nullopt};
    store->nextRename = BenchmarkRenameResult{false, BenchmarkWriteFailure::WriteFailed};
    const auto outcome = service.save({draft(token.id, "New", "after"), token});

    EXPECT_FALSE(outcome.ok());
    EXPECT_EQ(outcome.error, std::make_optional(BenchmarkSaveError::RenameFailed));
    ASSERT_TRUE(outcome.token.has_value());
    EXPECT_EQ(outcome.token->id, token.id);
    EXPECT_EQ(outcome.token->filename, "Old.json");
    EXPECT_EQ(outcome.token->digest, "d2");
    EXPECT_EQ(outcome.name, "Old");
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"Old.json"}));
    EXPECT_EQ(digests(*service.snapshot()), (std::vector<std::string>{"d2"}));
    const auto snapshotNow = service.snapshot();
    const auto *entry = loadedFor(*snapshotNow, "Old.json");
    EXPECT_EQ(markerOf(entry->benchmark), "after");
    EXPECT_EQ(entry->benchmark.name, "Old");

    store->nextWrite = BenchmarkWriteResult{std::string{"d3"}, std::nullopt};
    store->nextRename = BenchmarkRenameResult{true, std::nullopt};
    const auto retried = service.save({draft(token.id, "New", "after"), outcome.token});
    ASSERT_TRUE(retried.ok());
    EXPECT_EQ(store->lastWriteDigest, std::make_optional<std::string>("d2"));
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"New.json"}));
}

TEST(BenchmarksService, SaveReplacementRequiresMatchingAcceptedToken) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("Alpha.json", "d1")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    auto staleToken = tokenFor(service, "Alpha.json");
    staleToken.digest = "WRONG-DIGEST";
    const auto outcome = service.save(BenchmarkSaveRequest{draft(staleToken.id, "Alpha", "edited"), staleToken});

    EXPECT_FALSE(outcome.ok());
    EXPECT_EQ(outcome.error, std::make_optional(BenchmarkSaveError::StaleToken));
    EXPECT_EQ(store->writeCount, 0);
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"Alpha.json"}));
    EXPECT_EQ(digests(*service.snapshot()), (std::vector<std::string>{"d1"}));
    EXPECT_EQ(service.revision(), revisionBefore);
    EXPECT_EQ(callbacks, 0);
}

TEST(BenchmarksService, ASaveWhoseFileWasRenamedAwayIsAnUnknownTarget) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->scriptedScans = {okScan({loadedEntry("Alpha.json", "d1")})};
    store->nextScan = okScan({loadedEntry("Renamed.json", "d1")});
    BenchmarksService service(store);
    const auto token = tokenFor(service, "Alpha.json");

    service.refresh();
    const auto outcome = service.save({draft(token.id, "Alpha", "edited"), token});

    EXPECT_EQ(outcome.error, std::make_optional(BenchmarkSaveError::UnknownTarget));
    EXPECT_EQ(store->writeCount, 0);
}

TEST(BenchmarksService, SaveConflictOrWriteFailurePreservesAcceptedState) {
    for (const auto failure: {BenchmarkWriteFailure::ExternalModificationConflict,
                              BenchmarkWriteFailure::WriteFailed}) {
        auto store = std::make_shared<FakeBenchmarkStore>();
        store->nextScan = okScan({loadedEntry("Alpha.json", "d1", "before")});

        BenchmarksService service(store);
        int callbacks = 0;
        service.onChanged([&] { ++callbacks; });
        const auto revisionBefore = service.revision();

        store->nextWrite = BenchmarkWriteResult{std::nullopt, failure};
        const auto token = tokenFor(service, "Alpha.json");
        const auto outcome = service.save(BenchmarkSaveRequest{draft(token.id, "Renamed", "after"), token});

        EXPECT_FALSE(outcome.ok());
        ASSERT_TRUE(outcome.error.has_value());
        EXPECT_EQ(*outcome.error,
                  failure == BenchmarkWriteFailure::ExternalModificationConflict
                      ? BenchmarkSaveError::Conflict
                      : BenchmarkSaveError::WriteFailed);
        EXPECT_EQ(store->renameCount, 0);
        const auto snapshotNow = service.snapshot();
        const auto *entry = loadedFor(*snapshotNow, "Alpha.json");
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(markerOf(entry->benchmark), "before");
        EXPECT_EQ(service.snapshot()->entries.at(0).digest, "d1");
        EXPECT_EQ(service.revision(), revisionBefore);
        EXPECT_EQ(callbacks, 0);
    }
}

TEST(BenchmarksService, DeleteMatchingTokenRemovesEntryAndPublishes) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("b1.json", "d1"), loadedEntry("b2.json", "d2")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    store->nextRemove = BenchmarkDeleteResult{true, std::nullopt};
    const auto outcome = service.remove(tokenFor(service, "b1.json"));

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
    store->nextScan = okScan({loadedEntry("b1.json", "d1")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();
    const auto token = tokenFor(service, "b1.json");

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
    auto stale = service.remove(BenchmarkEditToken{token.id, "b1.json", "STALE"});
    EXPECT_FALSE(stale.ok());
    EXPECT_EQ(store->removeCount, 0);
    expectPreserved();

    // Matching token, but the store declines: classified outcome, accepted state intact.
    for (const auto failure: {BenchmarkWriteFailure::ExternalModificationConflict,
                              BenchmarkWriteFailure::WriteFailed}) {
        store->nextRemove = BenchmarkDeleteResult{false, failure};
        auto declined = service.remove(token);
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
    store->nextScan = okScan({loadedEntry("a.json", "da", "Aaa"), loadedEntry("b.json", "db", "Bbb"),
                              loadedEntry("c.json", "dc", "Ccc")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    store->scriptedWrites = {
        BenchmarkWriteResult{std::string{"da2"}, std::nullopt},
        BenchmarkWriteResult{std::nullopt, BenchmarkWriteFailure::ExternalModificationConflict},
        BenchmarkWriteResult{std::nullopt, BenchmarkWriteFailure::WriteFailed},
    };
    const auto a = tokenFor(service, "a.json");
    const auto b = tokenFor(service, "b.json");
    const auto c = tokenFor(service, "c.json");
    const std::vector<BenchmarkReplacementRequest> requests{
        {draft(a.id, "a", "Aaa mapped"), a},
        {draft(b.id, "b", "Bbb mapped"), b},
        {draft(c.id, "c", "Ccc mapped"), c},
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

    const auto snapshot = service.snapshot();
    EXPECT_EQ(markerOf(loadedFor(*snapshot, "a.json")->benchmark), "Aaa mapped");
    EXPECT_EQ(snapshot->entries.at(0).digest, "da2");
    EXPECT_EQ(markerOf(loadedFor(*snapshot, "b.json")->benchmark), "Bbb");  // conflict: untouched
    EXPECT_EQ(snapshot->entries.at(1).digest, "db");
    EXPECT_EQ(markerOf(loadedFor(*snapshot, "c.json")->benchmark), "Ccc");  // write failure: untouched
    EXPECT_EQ(snapshot->entries.at(2).digest, "dc");

    EXPECT_EQ(service.revision(), revisionBefore + 1);
    EXPECT_EQ(callbacks, 1);
}

TEST(BenchmarksService, BatchWritesInPlaceAndNeverRenamesOrCreatesEvenUnderAnUnusableName) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("foo .json", "d1", "before")});
    store->nextWrite = BenchmarkWriteResult{std::string{"d2"}, std::nullopt};
    BenchmarksService service(store);
    const auto token = tokenFor(service, "foo .json");

    const auto outcome = service.replaceBatch({{draft(token.id, "foo ", "mapped"), token}});

    ASSERT_TRUE(outcome.outcomes.at(0).ok());
    EXPECT_EQ(store->lastWriteFilename, "foo .json");
    EXPECT_EQ(store->lastWriteDigest, std::make_optional<std::string>("d1"));
    EXPECT_EQ(store->renameCount, 0);
    EXPECT_EQ(filenames(*service.snapshot()), (std::vector<std::string>{"foo .json"}));
    EXPECT_EQ(keyOf(service, "foo .json"), token.id);
}

TEST(BenchmarksService, BatchPublishesAtMostOnceAfterAllWrites) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("a.json", "da", "Aaa"), loadedEntry("b.json", "db", "Bbb")});

    BenchmarksService service(store);
    int callbacks = 0;
    std::vector<std::string> observedAtCallback;
    service.onChanged([&] {
        ++callbacks;
        observedAtCallback.clear();
        const auto snap = service.snapshot();
        if (snap)
            for (const auto &e: snap->entries)
                if (const auto *l = std::get_if<LoadedBenchmark>(&e.content))
                    observedAtCallback.push_back(markerOf(l->benchmark));
    });
    const auto revisionBefore = service.revision();

    store->scriptedWrites = {
        BenchmarkWriteResult{std::string{"da2"}, std::nullopt},
        BenchmarkWriteResult{std::string{"db2"}, std::nullopt},
    };
    const auto a = tokenFor(service, "a.json");
    const auto b = tokenFor(service, "b.json");
    service.replaceBatch({{draft(a.id, "a", "Aaa2"), a}, {draft(b.id, "b", "Bbb2"), b}});

    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(observedAtCallback, (std::vector<std::string>{"Aaa2", "Bbb2"}));
    EXPECT_EQ(service.revision(), revisionBefore + 1);
}

TEST(BenchmarksService, BatchWithNoSuccessDoesNotAdvanceAcceptedRevision) {
    auto store = std::make_shared<FakeBenchmarkStore>();
    store->nextScan = okScan({loadedEntry("a.json", "da", "Aaa")});

    BenchmarksService service(store);
    int callbacks = 0;
    service.onChanged([&] { ++callbacks; });
    const auto revisionBefore = service.revision();

    store->scriptedWrites = {BenchmarkWriteResult{std::nullopt, BenchmarkWriteFailure::WriteFailed}};
    const auto a = tokenFor(service, "a.json");
    const std::vector<BenchmarkReplacementRequest> requests{
        {draft(a.id, "a", "Aaa2"), a},                                                   // store fails
        {draft(domain::BenchmarkId{"bZ"}, "z", "Zzz"), {domain::BenchmarkId{"bZ"}, "z.json", "dz"}},  // unknown token
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
