#include <gtest/gtest.h>

#include <QColor>
#include <QSet>
#include <QUrl>
#include <QVariantMap>
#include <functional>
#include <memory>

#include "presentation/benchmark_manager_vm.h"
#include "usecases/benchmark_library_service.h"
#include "counting_ids.h"
#include "fake_benchmark_repository.h"
#include "fake_playlist_reader.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::presentation;
using namespace ksv::tests_support;

namespace {
    struct Fixture {
        std::shared_ptr<FakeBenchmarkRepository> repo = std::make_shared<FakeBenchmarkRepository>();
        std::shared_ptr<FakePlaylistReader> reader = std::make_shared<FakePlaylistReader>();
        std::shared_ptr<BenchmarkLibraryService> service;
        std::unique_ptr<BenchmarkManagerViewModel> vm;

        Fixture() {
            repo->nextScan = {BenchmarkLibrarySnapshot{}, std::nullopt};
            service = std::make_shared<BenchmarkLibraryService>(repo, reader, countingIds());
            vm = std::make_unique<BenchmarkManagerViewModel>(service);
        }
    };

    const BenchmarkScenarioNode *scenarioChild(const BenchmarkGroupNode *group, qsizetype index) {
        return qobject_cast<const BenchmarkScenarioNode *>(
            group->children().at(index).value<BenchmarkTreeNode *>());
    }

    const BenchmarkGroupNode *groupChild(const BenchmarkGroupNode *group, qsizetype index) {
        return qobject_cast<const BenchmarkGroupNode *>(
            group->children().at(index).value<BenchmarkTreeNode *>());
    }
}

TEST(BenchmarkManagerVm, BeginNewBenchmarkExposesDirtyIncompleteDraft) {
    Fixture f;
    f.vm->beginNewBenchmark();
    EXPECT_TRUE(f.vm->hasDraft());
    EXPECT_TRUE(f.vm->dirty());
    EXPECT_FALSE(f.vm->draftTrackable());
    EXPECT_FALSE(f.vm->draftFromLibrary());
    EXPECT_EQ(f.vm->benchmarkName(), QString());
}

TEST(BenchmarkManagerVm, LibraryEntriesReflectAcceptedSnapshotClassification) {
    Benchmark loaded;
    loaded.id = BenchmarkId{"bench-A"};
    loaded.name = "Alpha";
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back({"a.json", "d1", LoadedBenchmark{loaded, validateBenchmark(loaded)}});
    snapshot.entries.push_back({"bad.json", "d2", ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}}});
    Fixture f;
    f.repo->nextScan = {snapshot, std::nullopt};
    f.service->refresh();

    const auto entries = f.vm->libraryEntries();
    ASSERT_EQ(entries.size(), 2);
    const auto loadedRow = entries.at(0).toMap();
    EXPECT_EQ(loadedRow["classification"].toString(), "Incomplete");
    EXPECT_EQ(loadedRow["id"].toString(), "bench-A");
    EXPECT_EQ(loadedRow["name"].toString(), "Alpha");
    EXPECT_TRUE(loadedRow["openable"].toBool());
    EXPECT_TRUE(loadedRow["deletable"].toBool());
    const auto problemRow = entries.at(1).toMap();
    EXPECT_EQ(problemRow["classification"].toString(), "Invalid");
    EXPECT_EQ(problemRow["name"].toString(), "bad.json");
    EXPECT_FALSE(problemRow["openable"].toBool());
    EXPECT_FALSE(problemRow["deletable"].toBool());
}

TEST(BenchmarkManagerVm, ImportPlaylistFailureReturnsErrorMapAndLeavesNoDraft) {
    Fixture f;
    f.reader->nextResult = {std::nullopt, PlaylistImportFailure::MalformedJson};

    const auto result = f.vm->importPlaylist(QUrl("file:///tmp/playlist.json"));

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_FALSE(result["error"].toString().isEmpty());
    EXPECT_FALSE(f.vm->hasDraft());
}

TEST(BenchmarkManagerVm, ImportPlaylistSuccessSeedsDraftAndReportsSkippedDuplicates) {
    Fixture f;
    // PlaylistSeed.scenarioNames is already deduplicated (first occurrence, source order);
    // skippedDuplicateIndices records the source position the reader dropped.
    f.reader->nextResult = {PlaylistSeed{std::string{"Voltaic"}, {"a", "b", "c"}, {3}}, std::nullopt};

    const auto result = f.vm->importPlaylist(QUrl::fromLocalFile("/tmp/playlist.json"));

    EXPECT_TRUE(result["ok"].toBool());
    EXPECT_EQ(result["skipped"].toList().size(), 1);
    EXPECT_TRUE(f.vm->hasDraft());
    EXPECT_TRUE(f.vm->dirty());
    EXPECT_EQ(f.vm->benchmarkName(), "Voltaic");
    ASSERT_EQ(f.vm->root()->children().size(), 3);
    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->name(), "a");
    EXPECT_EQ(scenarioChild(f.vm->root(), 2)->name(), "c");
}

TEST(BenchmarkManagerVm, SaveEmptyNameSurfacesEmptyNameError) {
    Fixture f;
    f.vm->beginNewBenchmark();

    const auto result = f.vm->save();

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_FALSE(result["error"].toString().isEmpty());
}

TEST(BenchmarkManagerVm, SaveSuccessClearsDirtyAndRefreshesLibrary) {
    Fixture f;
    f.vm->beginNewBenchmark();
    f.vm->setBenchmarkName("Alpha");
    f.repo->nextWrite = {std::string{"digest-1"}, std::nullopt};

    const auto result = f.vm->save();

    EXPECT_TRUE(result["ok"].toBool());
    EXPECT_FALSE(f.vm->dirty());
    EXPECT_EQ(f.vm->libraryEntries().size(), 1);
    EXPECT_EQ(f.vm->libraryEntries().at(0).toMap()["name"].toString(), "Alpha");
}

TEST(BenchmarkManagerVm, ValidationIssuesMapCodesToText) {
    Fixture f;
    f.vm->beginNewBenchmark();

    const auto issues = f.vm->validationIssues();

    ASSERT_EQ(issues.size(), 3);  // a fresh draft: MissingName, NoScenarios, NoTiers
    QSet<QString> messages;
    for (const auto &issue: issues) {
        const auto row = issue.toMap();
        EXPECT_FALSE(row["message"].toString().isEmpty());
        EXPECT_EQ(row["targetId"].toString(), QString());
        messages.insert(row["message"].toString());
    }
    EXPECT_EQ(messages.size(), 3);
}

TEST(BenchmarkManagerVm, AddTierThenSetThresholdRebuildsTreeAndTiers) {
    Fixture f;
    f.vm->beginNewBenchmark();
    const auto scenario = f.vm->addUnplayedScenario("S");
    const auto tier = f.vm->addTier("Gold");
    ASSERT_TRUE(scenario["ok"].toBool());
    ASSERT_TRUE(tier["ok"].toBool());

    const auto threshold =
        f.vm->setThreshold(scenario["createdId"].toString(), tier["createdId"].toString(), 100.0);

    ASSERT_TRUE(threshold["ok"].toBool());
    ASSERT_EQ(f.vm->root()->children().size(), 1);
    const auto *node = scenarioChild(f.vm->root(), 0);
    EXPECT_EQ(node->nodeId(), scenario["createdId"].toString());
    ASSERT_EQ(node->thresholds().size(), 1);
    const auto stored = node->thresholds().at(0).toMap();
    EXPECT_EQ(stored["tierId"].toString(), tier["createdId"].toString());
    EXPECT_EQ(stored["tierName"].toString(), "Gold");
    EXPECT_EQ(stored["score"].toDouble(), 100.0);
    EXPECT_TRUE(stored["hasValue"].toBool());
    ASSERT_EQ(f.vm->tiers().size(), 1);
    EXPECT_EQ(f.vm->tiers().at(0).toMap()["name"].toString(), "Gold");
}

TEST(BenchmarkManagerVm, AddSubcategoryReorganizationReflectedInTree) {
    Fixture f;
    f.vm->beginNewBenchmark();
    const auto scenario = f.vm->addUnplayedScenario("S");
    const auto category = f.vm->addCategory("Clicking");
    ASSERT_TRUE(f.vm->moveScenario(scenario["createdId"].toString(), category["createdId"].toString())["ok"].toBool());

    const auto subcategory = f.vm->addSubcategory(category["createdId"].toString(), "Static");

    ASSERT_TRUE(subcategory["ok"].toBool());
    ASSERT_EQ(f.vm->root()->children().size(), 1);
    const auto *categoryNode = groupChild(f.vm->root(), 0);
    EXPECT_EQ(categoryNode->name(), "Clicking");
    // Atomic reorganization: the former direct scenario lands in an auto-created holder
    // subcategory, then the requested "Static" is appended — never a mixed shape.
    ASSERT_EQ(categoryNode->children().size(), 2);
    const auto *holderNode = groupChild(categoryNode, 0);
    ASSERT_EQ(holderNode->children().size(), 1);
    EXPECT_EQ(scenarioChild(holderNode, 0)->name(), "S");
    const auto *subNode = groupChild(categoryNode, 1);
    EXPECT_EQ(subNode->name(), "Static");
    EXPECT_EQ(subNode->children().size(), 0);
}

TEST(BenchmarkManagerVm, MoveScenarioToUncategorizedByEmptyTarget) {
    Fixture f;
    f.vm->beginNewBenchmark();
    const auto scenario = f.vm->addUnplayedScenario("S");
    const auto category = f.vm->addCategory("Clicking");
    ASSERT_TRUE(f.vm->moveScenario(scenario["createdId"].toString(), category["createdId"].toString())["ok"].toBool());
    ASSERT_EQ(f.vm->root()->children().size(), 1);  // only the category remains at the root

    const auto result = f.vm->moveScenario(scenario["createdId"].toString(), QString());

    ASSERT_TRUE(result["ok"].toBool());
    ASSERT_EQ(f.vm->root()->children().size(), 2);  // scenario back, then the category
    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->name(), "S");
    EXPECT_EQ(groupChild(f.vm->root(), 1)->children().size(), 0);
}

TEST(BenchmarkManagerVm, CommandErrorSurfacesInReturnMap) {
    Fixture f;

    const auto result = f.vm->addTier("Gold");

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_FALSE(result["error"].toString().isEmpty());
}

TEST(BenchmarkManagerVm, RefreshFailedSurfacesAfterDirectoryFailure) {
    Fixture f;
    f.repo->nextScan = {std::nullopt, BenchmarkScanFailure::DirectoryUnavailable};

    f.vm->refresh();

    EXPECT_TRUE(f.vm->refreshFailed());
    EXPECT_TRUE(f.vm->managedDirectoryPath() == QString::fromStdString(f.repo->directory));
}
