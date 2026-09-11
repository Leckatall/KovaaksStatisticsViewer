#include <gtest/gtest.h>

#include <QColor>
#include <QDateTime>
#include <QSet>
#include <QSignalSpy>
#include <QUrl>
#include <QVariantMap>
#include <chrono>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

#include "presentation/benchmark_manager_vm.h"
#include "contracts/benchmark_editor_seed.h"
#include "contracts/benchmark_manager_state.h"
#include "contracts/i_benchmark_manager_use_case.h"
#include "contracts/i_benchmark_resolution_use_case.h"
#include "benchmark_builders.h"
#include "fake_benchmark_manager_use_case.h"

using namespace ksv::application;
using namespace ksv::data;
using namespace ksv::domain;
using namespace ksv::presentation;
using namespace ksv::tests_support;

namespace {
    // M1: the manager view model's only dependency is the ADR-0004 use-case contract; no other
    // shared_ptr is interchangeable with IBenchmarkManagerUseCase.
    static_assert(std::is_constructible_v<BenchmarkManagerViewModel,
                                          std::shared_ptr<IBenchmarkManagerUseCase>>,
                  "BenchmarkManagerViewModel must construct from IBenchmarkManagerUseCase alone");
    static_assert(!std::is_constructible_v<BenchmarkManagerViewModel,
                                           std::shared_ptr<IBenchmarkResolutionUseCase>>,
                  "BenchmarkManagerViewModel must not accept a bare resolution use case");

    struct Fixture {
        std::shared_ptr<FakeBenchmarkManagerUseCase> uc =
            std::make_shared<FakeBenchmarkManagerUseCase>();
        std::unique_ptr<BenchmarkManagerViewModel> vm;

        void build() { vm = std::make_unique<BenchmarkManagerViewModel>(uc); }
        void publish() { uc->notify(); }
    };

    Tier tier(const std::string &id, const std::string &name) { return {TierId{id}, name, {}}; }

    Benchmark draftWith(const std::string &name, const std::vector<Tier> &tiers,
                        const std::vector<ScenarioEntry> &uncategorized) {
        Benchmark benchmark;
        benchmark.id = BenchmarkId{"b1"};
        benchmark.name = name;
        benchmark.tiers = tiers;
        benchmark.uncategorized = uncategorized;
        return benchmark;
    }

    BenchmarkEditorSeed newSeed(Benchmark benchmark) {
        return {std::move(benchmark), std::nullopt};
    }

    BenchmarkEditorSeed librarySeed(Benchmark benchmark) {
        auto id = benchmark.id;
        return {std::move(benchmark), BenchmarkEditToken{id, id.value + ".json", "d1"}};
    }

    BenchmarkLibrarySnapshot loadedSnapshot(const std::string &id, const std::string &name) {
        Benchmark benchmark;
        benchmark.id = BenchmarkId{id};
        benchmark.name = name;
        BenchmarkLibrarySnapshot snapshot;
        snapshot.entries.push_back(
            {id + ".json", "d1", LoadedBenchmark{benchmark, validateBenchmark(benchmark)}});
        return snapshot;
    }

    const BenchmarkScenarioNode *scenarioChild(const BenchmarkGroupNode *group, qsizetype index) {
        return qobject_cast<const BenchmarkScenarioNode *>(
            group->children().at(index).value<BenchmarkTreeNode *>());
    }

    const BenchmarkGroupNode *groupChild(const BenchmarkGroupNode *group, qsizetype index) {
        return qobject_cast<const BenchmarkGroupNode *>(
            group->children().at(index).value<BenchmarkTreeNode *>());
    }

    // No individual mutation command may reach the application boundary; only lifecycle commands do.
    void expectNoMutationForwarded(const FakeBenchmarkManagerUseCase &uc) {
        for (const auto &command: uc.commandLog)
            EXPECT_TRUE(command == "beginNewBenchmark" || command == "openBenchmark" ||
                        command == "closeEditor" || command == "importPlaylistSeed" ||
                        command == "save" || command == "deleteBenchmark" || command == "refresh")
                << "unexpected forwarded command: " << command;
    }
}

TEST(BenchmarkManagerVm, BeginNewBenchmarkCreatesLocalDirtyIncompleteWorkingCopy) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("", {}, {}));
    f.build();

    f.vm->beginNewBenchmark();

    EXPECT_TRUE(f.vm->hasDraft());
    EXPECT_TRUE(f.vm->dirty());
    EXPECT_FALSE(f.vm->draftTrackable());
    EXPECT_FALSE(f.vm->draftFromLibrary());
    EXPECT_EQ(f.vm->benchmarkName(), QString());
    ASSERT_FALSE(f.uc->commandLog.empty());
    EXPECT_EQ(f.uc->commandLog.back(), "beginNewBenchmark");
    expectNoMutationForwarded(*f.uc);
}

TEST(BenchmarkManagerVm, LibraryEntriesReflectAcceptedSnapshotClassification) {
    Benchmark loaded;
    loaded.id = BenchmarkId{"bench-A"};
    loaded.name = "Alpha";
    BenchmarkLibrarySnapshot snapshot;
    snapshot.entries.push_back({"a.json", "d1", LoadedBenchmark{loaded, validateBenchmark(loaded)}});
    snapshot.entries.push_back({"bad.json", "d2", ProblemBenchmark{BenchmarkFileProblem::Invalid, {}, {}, {}}});

    Fixture f;
    f.uc->stateValue.library = snapshot;
    f.build();

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

TEST(BenchmarkManagerVm, ImportPlaylistAdoptsSuccessfulSeedOnly) {
    Fixture f;
    f.uc->nextImport = {PlaylistImportFailure::MalformedJson, std::nullopt, {}};
    f.build();

    const auto failure = f.vm->importPlaylist(QUrl("file:///tmp/playlist.json"));
    EXPECT_FALSE(failure["ok"].toBool());
    EXPECT_FALSE(failure["error"].toString().isEmpty());
    EXPECT_FALSE(f.vm->hasDraft());

    f.uc->nextImport = {std::nullopt,
                        newSeed(draftWith("Voltaic", {},
                                          {benchmarkEntry("a", "a", std::nullopt, {}),
                                           benchmarkEntry("b", "b", std::nullopt, {}),
                                           benchmarkEntry("c", "c", std::nullopt, {})})),
                        {3}};

    const auto success = f.vm->importPlaylist(QUrl::fromLocalFile("/tmp/playlist.json"));
    EXPECT_TRUE(success["ok"].toBool());
    EXPECT_EQ(success["skipped"].toList().size(), 1);
    EXPECT_TRUE(f.vm->hasDraft());
    EXPECT_TRUE(f.vm->dirty());
    EXPECT_EQ(f.vm->benchmarkName(), "Voltaic");
    ASSERT_EQ(f.vm->root()->children().size(), 3);
    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->name(), "a");
    EXPECT_EQ(scenarioChild(f.vm->root(), 2)->name(), "c");
}

TEST(BenchmarkManagerVm, SaveEmptyNameSurfacesEmptyNameError) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("", {}, {}));
    f.uc->nextSaveOutcome = {std::nullopt, BenchmarkSaveError::EmptyName};
    f.build();
    f.vm->beginNewBenchmark();

    const auto result = f.vm->save();

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_FALSE(result["error"].toString().isEmpty());
}

TEST(BenchmarkManagerVm, SaveSuccessUpdatesLocalBaselineFromAdmittedToken) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Alpha", {}, {}));
    f.uc->nextSaveOutcome = {BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d1"}, std::nullopt};
    f.build();
    f.vm->beginNewBenchmark();
    ASSERT_TRUE(f.vm->dirty());

    const auto result = f.vm->save();

    EXPECT_TRUE(result["ok"].toBool());
    ASSERT_FALSE(f.uc->saveCalls.empty());
    EXPECT_EQ(f.uc->saveCalls.back().first.name, "Alpha");
    EXPECT_EQ(f.uc->commandLog.back(), "save");

    // Editor stays open; the admitted token becomes the new local baseline, so dirty clears.
    EXPECT_TRUE(f.vm->hasDraft());
    EXPECT_FALSE(f.vm->dirty());
    EXPECT_TRUE(f.vm->draftFromLibrary());

    // An external library republication does not re-dirty the saved copy.
    f.uc->stateValue.library = loadedSnapshot("b1", "Alpha");
    f.publish();
    EXPECT_FALSE(f.vm->dirty());

    // A further local edit re-dirties against the saved baseline.
    f.vm->setBenchmarkName("Alpha 2");
    EXPECT_TRUE(f.vm->dirty());
}

TEST(BenchmarkManagerVm, DiscardClosesAndForgetsUnsavedWorkingCopy) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Draft", {tier("g", "Gold")}, {}));
    f.build();
    f.vm->beginNewBenchmark();
    ASSERT_TRUE(f.vm->hasDraft());

    f.vm->discard();

    EXPECT_FALSE(f.vm->hasDraft());
    EXPECT_EQ(f.vm->benchmarkName(), QString());
    EXPECT_EQ(f.vm->draftId(), QString());
    EXPECT_EQ(f.vm->tiers().size(), 0);
    EXPECT_EQ(f.vm->root()->children().size(), 0);
    EXPECT_EQ(f.vm->validationIssues().size(), 0);
    EXPECT_EQ(f.uc->commandLog.back(), "closeEditor");
}

TEST(BenchmarkManagerVm, ValidationIssuesMapCodesToText) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("", {}, {}));
    f.build();
    f.vm->beginNewBenchmark();

    const auto issues = f.vm->validationIssues();

    ASSERT_EQ(issues.size(), 3);
    QSet<QString> messages;
    for (const auto &issue: issues) {
        const auto row = issue.toMap();
        EXPECT_FALSE(row["message"].toString().isEmpty());
        EXPECT_EQ(row["targetId"].toString(), QString());
        messages.insert(row["message"].toString());
    }
    EXPECT_EQ(messages.size(), 3);
}

TEST(BenchmarkManagerVm, DraftStateRendersScenariosTiersAndThresholds) {
    Fixture f;
    const auto entry = benchmarkEntry("e1", "S", std::nullopt, {{"gold", 100.0}});
    f.uc->nextNewSeed = newSeed(draftWith("B", {tier("gold", "Gold")}, {entry}));
    f.build();
    f.vm->beginNewBenchmark();

    ASSERT_EQ(f.vm->root()->children().size(), 1);
    const auto *node = scenarioChild(f.vm->root(), 0);
    EXPECT_EQ(node->nodeId(), "e1");
    ASSERT_EQ(node->thresholds().size(), 1);
    const auto stored = node->thresholds().at(0).toMap();
    EXPECT_EQ(stored["tierId"].toString(), "gold");
    EXPECT_EQ(stored["tierName"].toString(), "Gold");
    EXPECT_EQ(stored["score"].toDouble(), 100.0);
    EXPECT_TRUE(stored["hasValue"].toBool());
    ASSERT_EQ(f.vm->tiers().size(), 1);
    EXPECT_EQ(f.vm->tiers().at(0).toMap()["name"].toString(), "Gold");
}

TEST(BenchmarkManagerVm, EditorCommandsMutateLocallyThroughBenchmarkEditor) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("B", {tier("gold", "Gold")},
                                          {benchmarkEntry("e1", "S", std::nullopt, {})}));
    f.build();
    f.vm->beginNewBenchmark();
    ASSERT_FALSE(f.vm->dirty() == false);  // new draft is dirty from the start

    EXPECT_TRUE(f.vm->addUnplayedScenario("S2")["ok"].toBool());
    EXPECT_TRUE(f.vm->setThreshold("e1", "gold", 250.0)["ok"].toBool());
    EXPECT_TRUE(f.vm->addCategory("Clicking")["ok"].toBool());

    // Local benchmark and its projections changed, driven by domain::BenchmarkEditor.
    ASSERT_EQ(f.vm->root()->children().size(), 3);  // S, S2 (uncategorized) + the new Clicking category
    EXPECT_EQ(scenarioChild(f.vm->root(), 1)->name(), "S2");
    EXPECT_EQ(groupChild(f.vm->root(), 2)->name(), "Clicking");
    const auto stored = scenarioChild(f.vm->root(), 0)->thresholds().at(0).toMap();
    EXPECT_EQ(stored["score"].toDouble(), 250.0);
    EXPECT_TRUE(stored["hasValue"].toBool());
    EXPECT_TRUE(f.vm->dirty());

    // No individual mutation command reached the application boundary.
    expectNoMutationForwarded(*f.uc);
}

TEST(BenchmarkManagerVm, CategorySubcategoryHierarchyReflectedInTree) {
    Fixture f;
    Benchmark benchmark;
    benchmark.id = BenchmarkId{"b1"};
    benchmark.name = "B";
    Category category{GroupId{"c1"}, "Clicking", {}, {}, {}};
    Subcategory holder{GroupId{"h1"}, "Holder", {}, {}};
    holder.scenarios.push_back(benchmarkEntry("e1", "S", std::nullopt, {}));
    Subcategory statics{GroupId{"s1"}, "Static", {}, {}};
    category.subcategories = {holder, statics};
    benchmark.categories = {category};
    f.uc->nextNewSeed = newSeed(benchmark);
    f.build();
    f.vm->beginNewBenchmark();

    ASSERT_EQ(f.vm->root()->children().size(), 1);
    const auto *categoryNode = groupChild(f.vm->root(), 0);
    EXPECT_EQ(categoryNode->name(), "Clicking");
    ASSERT_EQ(categoryNode->children().size(), 2);
    const auto *holderNode = groupChild(categoryNode, 0);
    ASSERT_EQ(holderNode->children().size(), 1);
    EXPECT_EQ(scenarioChild(holderNode, 0)->name(), "S");
    EXPECT_EQ(groupChild(categoryNode, 1)->children().size(), 0);

    EXPECT_TRUE(f.vm->addSubcategory("c1", "Dynamic")["ok"].toBool());
    ASSERT_EQ(groupChild(f.vm->root(), 0)->children().size(), 3);
    EXPECT_EQ(groupChild(groupChild(f.vm->root(), 0), 2)->name(), "Dynamic");
    expectNoMutationForwarded(*f.uc);
}

TEST(BenchmarkManagerVm, MoveScenarioMutatesLocalWorkingCopy) {
    Fixture f;
    Benchmark benchmark = draftWith("B", {}, {benchmarkEntry("e1", "S", std::nullopt, {})});
    benchmark.categories = {Category{GroupId{"c1"}, "Clicking", {}, {}, {}}};
    f.uc->nextNewSeed = newSeed(benchmark);
    f.build();
    f.vm->beginNewBenchmark();

    EXPECT_TRUE(f.vm->moveScenario("e1", "c1")["ok"].toBool());
    ASSERT_EQ(f.vm->root()->children().size(), 1);  // only the category remains at top level
    const auto *category = groupChild(f.vm->root(), 0);
    ASSERT_NE(category, nullptr);
    ASSERT_EQ(category->children().size(), 1);
    EXPECT_EQ(scenarioChild(category, 0)->name(), "S");

    EXPECT_TRUE(f.vm->moveScenario("e1", QString())["ok"].toBool());  // empty target = uncategorized
    ASSERT_EQ(f.vm->root()->children().size(), 2);  // scenario back at top level + the category node
    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->name(), "S");
    expectNoMutationForwarded(*f.uc);
}

TEST(BenchmarkManagerVm, EditorCommandErrorSurfacesInReturnMap) {
    Fixture f;
    f.build();

    // No working copy open: a mutation invokable returns a non-empty error map.
    const auto noDraft = f.vm->addTier("Gold");
    EXPECT_FALSE(noDraft["ok"].toBool());
    EXPECT_FALSE(noDraft["error"].toString().isEmpty());

    // With a working copy, a genuine BenchmarkEditor error is surfaced verbatim in the map.
    f.uc->nextNewSeed = newSeed(draftWith("B", {}, {benchmarkEntry("e1", "S", std::nullopt, {})}));
    f.vm->beginNewBenchmark();
    const auto duplicate = f.vm->addUnplayedScenario("S");
    EXPECT_FALSE(duplicate["ok"].toBool());
    EXPECT_FALSE(duplicate["error"].toString().isEmpty());
    expectNoMutationForwarded(*f.uc);
}

TEST(BenchmarkManagerVm, RefreshFailedSurfacesAfterDirectoryFailure) {
    Fixture f;
    f.uc->stateValue.refreshFailed = true;
    f.uc->stateValue.managedDirectoryPath = "C:/Benchmarks";
    f.uc->managedDirectory = "C:/Benchmarks";
    f.build();

    EXPECT_TRUE(f.vm->refreshFailed());
    EXPECT_EQ(f.vm->managedDirectoryPath(), QString::fromStdString("C:/Benchmarks"));

    f.vm->refresh();
    ASSERT_FALSE(f.uc->commandLog.empty());
    EXPECT_EQ(f.uc->commandLog.back(), "refresh");
}

TEST(BenchmarkManagerVm, OneNotificationInstallsOneCoherentRevision) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Second", {tier("t1", "Gold"), tier("t2", "Plat")},
                                          {benchmarkEntry("e1", "New", std::string{"h1"}, {})}));
    f.uc->nextResolveResult = {resolvedEntry("e1", "h1")};
    f.uc->stateValue.scenarioCatalogue = {ScenarioId{"Old", "h0"}};
    f.build();
    f.vm->beginNewBenchmark();

    QSignalSpy draftSpy(f.vm.get(), &BenchmarkManagerViewModel::draftChanged);
    QSignalSpy librarySpy(f.vm.get(), &BenchmarkManagerViewModel::libraryChanged);

    BenchmarkManagerState next;
    next.scenarioCatalogue = {ScenarioId{"New", "h1"}, ScenarioId{"Zeta", "hz"}};
    next.resolutionWriteFailed = true;
    next.library = loadedSnapshot("b1", "Second");
    f.uc->stateValue = next;
    f.publish();

    EXPECT_GE(draftSpy.count(), 1);
    EXPECT_GE(librarySpy.count(), 1);
    EXPECT_EQ(f.vm->benchmarkName(), "Second");
    EXPECT_TRUE(f.vm->dirty());
    ASSERT_EQ(f.vm->tiers().size(), 2);
    EXPECT_EQ(f.vm->tiers().at(0).toMap()["name"].toString(), "Gold");
    ASSERT_EQ(f.vm->root()->children().size(), 1);
    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->name(), "New");
    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->matchState(), QStringLiteral("resolved"));
    ASSERT_EQ(f.vm->scenarioCatalogue().size(), 2);
    EXPECT_EQ(f.vm->scenarioCatalogue().at(1).toMap()["name"].toString(), "Zeta");
    EXPECT_TRUE(f.vm->resolutionWriteFailed());
    ASSERT_EQ(f.vm->libraryEntries().size(), 1);
}

TEST(BenchmarkManagerResolution, ScenarioNodesCarryTheirMatchState) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Saved", {},
                                          {benchmarkEntry("e1", "Alpha", std::string{"h1"}, {}),
                                           benchmarkEntry("e2", "Beta", std::nullopt, {}),
                                           benchmarkEntry("e3", "Gamma", std::string{"gone"}, {})}));
    f.uc->nextResolveResult = {resolvedEntry("e1", "h1"), unresolvedEntry("e2"),
                               mappedUnavailableEntry("e3", "gone")};
    f.build();
    f.vm->beginNewBenchmark();

    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->matchState(), QStringLiteral("resolved"));
    EXPECT_EQ(scenarioChild(f.vm->root(), 1)->matchState(), QStringLiteral("unresolved"));
    EXPECT_EQ(scenarioChild(f.vm->root(), 2)->matchState(), QStringLiteral("mappedUnavailable"));
}

TEST(BenchmarkManagerResolution, AnAmbiguousNodeExposesItsCandidates) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Saved", {}, {benchmarkEntry("e1", "Alpha", std::nullopt, {})}));
    ScenarioResolution ambiguous{ScenarioEntryId{"e1"}, ScenarioMatchState::Ambiguous, std::nullopt, {}};
    ambiguous.candidates.push_back(
        {"ha", 3, std::chrono::sys_seconds{std::chrono::seconds{1'700'000'000}}});
    ambiguous.candidates.push_back({"hb", 7, std::nullopt});
    f.uc->nextResolveResult = {ambiguous};
    f.build();
    f.vm->beginNewBenchmark();

    const auto *node = scenarioChild(f.vm->root(), 0);
    EXPECT_EQ(node->matchState(), QStringLiteral("ambiguous"));
    ASSERT_EQ(node->candidates().size(), 2);
    const auto candidate = node->candidates().at(0).toMap();
    EXPECT_EQ(candidate.value("hash").toString(), QStringLiteral("ha"));
    EXPECT_EQ(candidate.value("runCount").toInt(), 3);
    EXPECT_EQ(candidate.value("lastPlayed").toDateTime().toSecsSinceEpoch(), 1'700'000'000);
    EXPECT_FALSE(node->candidates().at(1).toMap().value("lastPlayed").toDateTime().isValid());
}

TEST(BenchmarkManagerResolution, AnAutoMappableNodeReportsItsSingleCandidate) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Saved", {}, {benchmarkEntry("e1", "Alpha", std::nullopt, {})}));
    ScenarioResolution mappable{ScenarioEntryId{"e1"}, ScenarioMatchState::AutoMappable, std::nullopt, {}};
    mappable.candidates.push_back({"h1", 1, std::nullopt});
    f.uc->nextResolveResult = {mappable};
    f.uc->stateValue.resolutionWriteFailed = true;
    f.build();
    f.vm->beginNewBenchmark();

    const auto *node = scenarioChild(f.vm->root(), 0);
    EXPECT_EQ(node->matchState(), QStringLiteral("autoMappable"));
    ASSERT_EQ(node->candidates().size(), 1);
    EXPECT_EQ(node->candidates().at(0).toMap().value("hash").toString(), QStringLiteral("h1"));
    EXPECT_TRUE(f.vm->resolutionWriteFailed());
}

TEST(BenchmarkManagerResolution, TheScenarioCatalogueIsExposedForThePicker) {
    Fixture f;
    f.uc->stateValue.scenarioCatalogue = {ScenarioId{"Alpha", "ha"}, ScenarioId{"Alpha", "hb"},
                                          ScenarioId{"Beta", "h2"}};
    f.build();

    const auto catalogue = f.vm->scenarioCatalogue();

    ASSERT_EQ(catalogue.size(), 3);
    EXPECT_EQ(catalogue.at(0).toMap().value("name").toString(), QStringLiteral("Alpha"));
    EXPECT_EQ(catalogue.at(0).toMap().value("hash").toString(), QStringLiteral("ha"));
    EXPECT_EQ(catalogue.at(1).toMap().value("hash").toString(), QStringLiteral("hb"));
    EXPECT_EQ(catalogue.at(2).toMap().value("name").toString(), QStringLiteral("Beta"));
}

TEST(BenchmarkManagerResolution, SetScenarioHashMutatesLocalWorkingCopy) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Saved", {}, {benchmarkEntry("e1", "Alpha", std::nullopt, {})}));
    f.build();
    f.vm->beginNewBenchmark();

    const auto result = f.vm->setScenarioHash("e1", "hb");

    EXPECT_TRUE(result.value("ok").toBool());
    EXPECT_TRUE(scenarioChild(f.vm->root(), 0)->hasHash());
    EXPECT_TRUE(f.vm->dirty());
    expectNoMutationForwarded(*f.uc);
}

TEST(BenchmarkManagerResolution, SetScenarioHashEmptyClearsLocalMapping) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Saved", {},
                                          {benchmarkEntry("e1", "Alpha", std::string{"h-old"}, {})}));
    f.build();
    f.vm->beginNewBenchmark();
    ASSERT_TRUE(scenarioChild(f.vm->root(), 0)->hasHash());

    EXPECT_TRUE(f.vm->setScenarioHash("e1", "").value("ok").toBool());

    EXPECT_FALSE(scenarioChild(f.vm->root(), 0)->hasHash());
    expectNoMutationForwarded(*f.uc);
}

TEST(BenchmarkManagerResolution, SetScenarioHashReportsAnUnknownEntry) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Saved", {}, {benchmarkEntry("e1", "Alpha", std::nullopt, {})}));
    f.build();
    f.vm->beginNewBenchmark();

    const auto result = f.vm->setScenarioHash("nope", "h1");

    EXPECT_FALSE(result.value("ok").toBool());
    EXPECT_FALSE(result.value("error").toString().isEmpty());
}

TEST(BenchmarkManagerResolution, ScenarioNodesOutsideUncategorizedAlsoCarryTheirState) {
    Fixture f;
    Benchmark benchmark;
    benchmark.id = BenchmarkId{"b1"};
    benchmark.name = "Saved";
    Category category{GroupId{"c1"}, "C", {}, {}, {}};
    category.scenarios.push_back(benchmarkEntry("d1", "Nested", std::string{"hn"}, {}));
    benchmark.categories.push_back(category);
    f.uc->nextNewSeed = newSeed(benchmark);
    f.uc->nextResolveResult = {resolvedEntry("d1", "hn")};
    f.build();
    f.vm->beginNewBenchmark();

    const auto *categoryNode = groupChild(f.vm->root(), 0);
    EXPECT_EQ(scenarioChild(categoryNode, 0)->matchState(), QStringLiteral("resolved"));
}

// ---- Task P5: refresh preservation, stale baseline, conflict-safe lifecycle -------------------

TEST(BenchmarkManagerVm, RefreshWhileEditingPreservesWorkingCopy) {
    Fixture f;
    Benchmark accepted = draftWith("Alpha", {tier("gold", "Gold")},
                                   {benchmarkEntry("e1", "S", std::nullopt, {})});
    f.uc->stateValue.library = [&] {
        BenchmarkLibrarySnapshot s;
        s.entries.push_back({"b1.json", "d1", LoadedBenchmark{accepted, validateBenchmark(accepted)}});
        return s;
    }();
    f.uc->nextOpenSeed = librarySeed(accepted);
    f.build();
    ASSERT_TRUE(f.vm->openBenchmark("b1"));
    f.vm->setBenchmarkName("Alpha edited");
    ASSERT_TRUE(f.vm->dirty());
    const auto issuesBefore = f.vm->validationIssues().size();

    // A later accepted snapshot with different content is published.
    Benchmark changed = accepted;
    changed.name = "Alpha (renamed on disk)";
    BenchmarkLibrarySnapshot next;
    next.entries.push_back({"b1.json", "d2", LoadedBenchmark{changed, validateBenchmark(changed)}});
    f.uc->stateValue.library = next;
    f.publish();

    EXPECT_EQ(f.vm->benchmarkName(), "Alpha edited");   // working value + local edit preserved
    EXPECT_TRUE(f.vm->dirty());
    EXPECT_EQ(f.vm->validationIssues().size(), issuesBefore);
    ASSERT_EQ(f.vm->libraryEntries().size(), 1);        // library rows reflect the new snapshot
    EXPECT_EQ(f.vm->libraryEntries().at(0).toMap()["name"].toString(), "Alpha (renamed on disk)");
}

TEST(BenchmarkManagerVm, ChangedRemovedOrReclassifiedBaselineBecomesStale) {
    Fixture f;
    Benchmark accepted = draftWith("Alpha", {}, {benchmarkEntry("e1", "S", std::nullopt, {})});
    const auto seedSnapshot = [&](const std::string &digest) {
        BenchmarkLibrarySnapshot s;
        s.entries.push_back({"b1.json", digest,
                             LoadedBenchmark{accepted, validateBenchmark(accepted)}});
        return s;
    };
    f.uc->stateValue.library = seedSnapshot("d1");
    f.uc->nextOpenSeed = librarySeed(accepted);
    f.build();
    ASSERT_TRUE(f.vm->openBenchmark("b1"));
    EXPECT_FALSE(f.vm->baselineStale());

    // Digest changes under the open editor.
    f.uc->stateValue.library = seedSnapshot("d2");
    f.publish();
    EXPECT_TRUE(f.vm->baselineStale());
    EXPECT_TRUE(f.vm->hasDraft());                       // not rebased or closed
    EXPECT_EQ(f.vm->benchmarkName(), "Alpha");

    // Entry removed entirely.
    f.uc->stateValue.library = BenchmarkLibrarySnapshot{};
    f.publish();
    EXPECT_TRUE(f.vm->baselineStale());
    EXPECT_TRUE(f.vm->hasDraft());

    // Entry reclassified as a problem entry.
    BenchmarkLibrarySnapshot invalid;
    invalid.entries.push_back({"b1.json", "d3",
                               ProblemBenchmark{BenchmarkFileProblem::Invalid, std::nullopt,
                                                BenchmarkId{"b1"}, std::nullopt}});
    f.uc->stateValue.library = invalid;
    f.publish();
    EXPECT_TRUE(f.vm->baselineStale());
    EXPECT_TRUE(f.vm->hasDraft());
}

TEST(BenchmarkManagerVm, SaveConflictKeepsWorkingCopyAndOriginalBaseline) {
    Fixture f;
    Benchmark accepted = draftWith("Alpha", {}, {benchmarkEntry("e1", "S", std::nullopt, {})});
    BenchmarkLibrarySnapshot s;
    s.entries.push_back({"b1.json", "d1", LoadedBenchmark{accepted, validateBenchmark(accepted)}});
    f.uc->stateValue.library = s;
    f.uc->nextOpenSeed = librarySeed(accepted);
    f.build();
    ASSERT_TRUE(f.vm->openBenchmark("b1"));
    f.vm->setBenchmarkName("Alpha edited");

    // The accepted digest moves, marking the baseline stale.
    BenchmarkLibrarySnapshot moved;
    moved.entries.push_back({"b1.json", "d2", LoadedBenchmark{accepted, validateBenchmark(accepted)}});
    f.uc->stateValue.library = moved;
    f.publish();
    ASSERT_TRUE(f.vm->baselineStale());

    f.uc->nextSaveOutcome = {std::nullopt, BenchmarkSaveError::Conflict};
    const auto result = f.vm->save();

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_FALSE(result["error"].toString().isEmpty());
    EXPECT_EQ(f.vm->benchmarkName(), "Alpha edited");   // local content still editable / intact
    EXPECT_TRUE(f.vm->dirty());
    EXPECT_TRUE(f.vm->baselineStale());
    ASSERT_FALSE(f.uc->saveCalls.empty());
    EXPECT_EQ(f.uc->saveCalls.back().second->digest, "d1");  // original admitted token preserved
}

TEST(BenchmarkManagerVm, DeleteDoesNotSilentlyDestroyOpenWorkingCopy) {
    for (const auto outcome : {std::optional<BenchmarkRemoveError>{},
                               std::optional<BenchmarkRemoveError>{BenchmarkRemoveError::Conflict},
                               std::optional<BenchmarkRemoveError>{BenchmarkRemoveError::WriteFailed}}) {
        Fixture f;
        Benchmark accepted = draftWith("Alpha", {}, {benchmarkEntry("e1", "S", std::nullopt, {})});
        BenchmarkLibrarySnapshot s;
        s.entries.push_back({"b1.json", "d1", LoadedBenchmark{accepted, validateBenchmark(accepted)}});
        f.uc->stateValue.library = s;
        f.uc->nextOpenSeed = librarySeed(accepted);
        f.build();
        ASSERT_TRUE(f.vm->openBenchmark("b1"));
        f.uc->nextRemoveOutcome = {outcome};

        f.vm->deleteBenchmark("b1");

        EXPECT_TRUE(f.vm->hasDraft());                  // never silently destroyed
        EXPECT_EQ(f.vm->benchmarkName(), "Alpha");

        if (!outcome.has_value()) {
            // A successful delete removes the accepted entry; the next publication marks the
            // still-open working copy's baseline stale.
            f.uc->stateValue.library = BenchmarkLibrarySnapshot{};
            f.publish();
            EXPECT_TRUE(f.vm->hasDraft());
            EXPECT_TRUE(f.vm->baselineStale());
        } else {
            // A failed delete leaves accepted and presentation state untouched.
            EXPECT_FALSE(f.vm->baselineStale());
        }
    }
}
