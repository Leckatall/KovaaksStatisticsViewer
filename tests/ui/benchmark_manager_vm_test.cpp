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
#include "contracts/benchmark_manager_state.h"
#include "contracts/i_benchmark_library_service.h"
#include "contracts/i_benchmark_manager_use_case.h"
#include "benchmark_builders.h"
#include "fake_benchmark_manager_use_case.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::presentation;
using namespace ksv::tests_support;

namespace {
    // M1: the manager view model's only dependency is the ADR-0004 use-case contract; the former
    // IBenchmarkLibraryService constructor must be gone, so neither shared_ptr is interchangeable.
    static_assert(std::is_constructible_v<BenchmarkManagerViewModel,
                                          std::shared_ptr<IBenchmarkManagerUseCase>>,
                  "BenchmarkManagerViewModel must construct from IBenchmarkManagerUseCase alone");
    static_assert(!std::is_constructible_v<BenchmarkManagerViewModel,
                                           std::shared_ptr<IBenchmarkLibraryService>>,
                  "BenchmarkManagerViewModel must not accept IBenchmarkLibraryService");

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
}

TEST(BenchmarkManagerVm, BeginNewBenchmarkExposesDirtyIncompleteDraft) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("", {}, {});
    f.uc->stateValue.draftDirty = true;
    f.uc->stateValue.draftFromLibrary = false;
    f.uc->stateValue.draftCompleteness.completeness = Completeness::Incomplete;
    f.build();

    EXPECT_TRUE(f.vm->hasDraft());
    EXPECT_TRUE(f.vm->dirty());
    EXPECT_FALSE(f.vm->draftTrackable());
    EXPECT_FALSE(f.vm->draftFromLibrary());
    EXPECT_EQ(f.vm->benchmarkName(), QString());

    f.vm->beginNewBenchmark();
    ASSERT_FALSE(f.uc->commandLog.empty());
    EXPECT_EQ(f.uc->commandLog.back(), "beginNewDraft");
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

TEST(BenchmarkManagerVm, ImportPlaylistFailureReturnsErrorMapAndLeavesNoDraft) {
    Fixture f;
    f.uc->importResult = {PlaylistImportFailure::MalformedJson, {}};
    f.build();

    const auto result = f.vm->importPlaylist(QUrl("file:///tmp/playlist.json"));

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_FALSE(result["error"].toString().isEmpty());
    EXPECT_FALSE(f.vm->hasDraft());
}

TEST(BenchmarkManagerVm, ImportPlaylistSuccessSeedsDraftAndReportsSkippedDuplicates) {
    Fixture f;
    f.uc->importResult = {std::nullopt, {3}};
    f.build();

    const auto result = f.vm->importPlaylist(QUrl::fromLocalFile("/tmp/playlist.json"));

    EXPECT_TRUE(result["ok"].toBool());
    EXPECT_EQ(result["skipped"].toList().size(), 1);

    // The seeded draft is not part of the command result; it lands on the next publication.
    f.uc->stateValue.draft = draftWith("Voltaic", {},
                                       {benchmarkEntry("a", "a", std::nullopt, {}),
                                        benchmarkEntry("b", "b", std::nullopt, {}),
                                        benchmarkEntry("c", "c", std::nullopt, {})});
    f.uc->stateValue.draftDirty = true;
    f.publish();

    EXPECT_TRUE(f.vm->hasDraft());
    EXPECT_TRUE(f.vm->dirty());
    EXPECT_EQ(f.vm->benchmarkName(), "Voltaic");
    ASSERT_EQ(f.vm->root()->children().size(), 3);
    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->name(), "a");
    EXPECT_EQ(scenarioChild(f.vm->root(), 2)->name(), "c");
}

TEST(BenchmarkManagerVm, SaveEmptyNameSurfacesEmptyNameError) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("", {}, {});
    f.uc->saveResult = {BenchmarkSaveError::EmptyName};
    f.build();

    const auto result = f.vm->save();

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_FALSE(result["error"].toString().isEmpty());
}

TEST(BenchmarkManagerVm, SaveSuccessClearsDirtyAndRefreshesLibrary) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("Alpha", {}, {});
    f.uc->stateValue.draftDirty = true;
    f.build();

    const auto result = f.vm->save();

    EXPECT_TRUE(result["ok"].toBool());
    ASSERT_FALSE(f.uc->commandLog.empty());
    EXPECT_EQ(f.uc->commandLog.back(), "saveDraft");

    // A successful save drops the draft and republishes the accepted library.
    f.uc->stateValue.draft.reset();
    f.uc->stateValue.draftDirty = false;
    f.uc->stateValue.library = loadedSnapshot("b1", "Alpha");
    f.publish();

    EXPECT_FALSE(f.vm->dirty());
    ASSERT_EQ(f.vm->libraryEntries().size(), 1);
    EXPECT_EQ(f.vm->libraryEntries().at(0).toMap()["name"].toString(), "Alpha");
}

TEST(BenchmarkManagerVm, ValidationIssuesMapCodesToText) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("", {}, {});
    f.uc->stateValue.draftCompleteness.issues = {
        {BenchmarkIssueCode::MissingName, std::monostate{}},
        {BenchmarkIssueCode::NoScenarios, std::monostate{}},
        {BenchmarkIssueCode::NoTiers, std::monostate{}},
    };
    f.build();

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
    f.uc->stateValue.draft = draftWith("B", {tier("gold", "Gold")}, {entry});
    f.build();

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

TEST(BenchmarkManagerVm, SetThresholdForwardsToTheUseCase) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("B", {tier("gold", "Gold")},
                                       {benchmarkEntry("e1", "S", std::nullopt, {})});
    f.build();

    const auto result = f.vm->setThreshold("e1", "gold", 250.0);

    EXPECT_TRUE(result["ok"].toBool());
    ASSERT_EQ(f.uc->setThresholdArgs.size(), 1u);
    EXPECT_EQ(std::get<0>(f.uc->setThresholdArgs[0]), "e1");
    EXPECT_EQ(std::get<1>(f.uc->setThresholdArgs[0]), "gold");
    EXPECT_DOUBLE_EQ(std::get<2>(f.uc->setThresholdArgs[0]), 250.0);
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
    f.uc->stateValue.draft = benchmark;
    f.build();

    ASSERT_EQ(f.vm->root()->children().size(), 1);
    const auto *categoryNode = groupChild(f.vm->root(), 0);
    EXPECT_EQ(categoryNode->name(), "Clicking");
    ASSERT_EQ(categoryNode->children().size(), 2);
    const auto *holderNode = groupChild(categoryNode, 0);
    ASSERT_EQ(holderNode->children().size(), 1);
    EXPECT_EQ(scenarioChild(holderNode, 0)->name(), "S");
    EXPECT_EQ(groupChild(categoryNode, 1)->children().size(), 0);

    f.vm->addSubcategory("c1", "Static");
    ASSERT_EQ(f.uc->addSubcategoryArgs.size(), 1u);
    EXPECT_EQ(f.uc->addSubcategoryArgs[0].first, "c1");
    EXPECT_EQ(f.uc->addSubcategoryArgs[0].second, "Static");
}

TEST(BenchmarkManagerVm, MoveScenarioForwardsGroupTargetAndUncategorizedByEmptyTarget) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("B", {}, {benchmarkEntry("e1", "S", std::nullopt, {})});
    f.build();

    f.vm->moveScenario("e1", "c1");
    f.vm->moveScenario("e1", QString());

    ASSERT_EQ(f.uc->moveScenarioArgs.size(), 2u);
    EXPECT_EQ(f.uc->moveScenarioArgs[0].first, "e1");
    ASSERT_TRUE(std::holds_alternative<GroupId>(f.uc->moveScenarioArgs[0].second));
    EXPECT_EQ(std::get<GroupId>(f.uc->moveScenarioArgs[0].second).value, "c1");
    EXPECT_TRUE(std::holds_alternative<std::monostate>(f.uc->moveScenarioArgs[1].second));
}

TEST(BenchmarkManagerVm, CommandErrorSurfacesInReturnMap) {
    Fixture f;
    f.uc->scriptedDraftResults["addTier"] = {BenchmarkDraftError::UnknownTier};
    f.build();

    const auto result = f.vm->addTier("Gold");

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_FALSE(result["error"].toString().isEmpty());
    ASSERT_EQ(f.uc->addTierNames.size(), 1u);
    EXPECT_EQ(f.uc->addTierNames[0], "Gold");
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
    f.uc->stateValue.draft =
        draftWith("First", {tier("t0", "Bronze")}, {benchmarkEntry("e0", "Old", std::nullopt, {})});
    f.uc->stateValue.scenarioCatalogue = {ScenarioId{"Old", "h0"}};
    f.build();

    QSignalSpy draftSpy(f.vm.get(), &BenchmarkManagerViewModel::draftChanged);
    QSignalSpy librarySpy(f.vm.get(), &BenchmarkManagerViewModel::libraryChanged);

    // One publication is one coherent revision: name, ladder, tree, catalogue and the
    // automatic-resolution failure flag all come from the same state() read and must land
    // together, not from per-field service reads staggered across callbacks.
    BenchmarkManagerState next;
    next.draft = draftWith("Second", {tier("t1", "Gold"), tier("t2", "Plat")},
                           {benchmarkEntry("e1", "New", std::string{"h1"}, {})});
    next.draftDirty = true;
    next.draftResolutions = {resolvedEntry("e1", "h1")};
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
    f.uc->stateValue.draft = draftWith("Saved", {},
                                       {benchmarkEntry("e1", "Alpha", std::string{"h1"}, {}),
                                        benchmarkEntry("e2", "Beta", std::nullopt, {}),
                                        benchmarkEntry("e3", "Gamma", std::string{"gone"}, {})});
    f.uc->stateValue.draftResolutions = {resolvedEntry("e1", "h1"), unresolvedEntry("e2"),
                                         mappedUnavailableEntry("e3", "gone")};
    f.build();

    EXPECT_EQ(scenarioChild(f.vm->root(), 0)->matchState(), QStringLiteral("resolved"));
    EXPECT_EQ(scenarioChild(f.vm->root(), 1)->matchState(), QStringLiteral("unresolved"));
    EXPECT_EQ(scenarioChild(f.vm->root(), 2)->matchState(), QStringLiteral("mappedUnavailable"));
}

TEST(BenchmarkManagerResolution, AnAmbiguousNodeExposesItsCandidates) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("Saved", {}, {benchmarkEntry("e1", "Alpha", std::nullopt, {})});
    ScenarioResolution ambiguous{ScenarioEntryId{"e1"}, ScenarioMatchState::Ambiguous, std::nullopt, {}};
    ambiguous.candidates.push_back(
        {"ha", 3, std::chrono::sys_seconds{std::chrono::seconds{1'700'000'000}}});
    ambiguous.candidates.push_back({"hb", 7, std::nullopt});
    f.uc->stateValue.draftResolutions = {ambiguous};
    f.build();

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
    f.uc->stateValue.draft = draftWith("Saved", {}, {benchmarkEntry("e1", "Alpha", std::nullopt, {})});
    ScenarioResolution mappable{ScenarioEntryId{"e1"}, ScenarioMatchState::AutoMappable, std::nullopt, {}};
    mappable.candidates.push_back({"h1", 1, std::nullopt});
    f.uc->stateValue.draftResolutions = {mappable};
    f.uc->stateValue.resolutionWriteFailed = true;
    f.build();

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

TEST(BenchmarkManagerResolution, SetScenarioHashForwardsAChosenCandidate) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("Saved", {}, {benchmarkEntry("e1", "Alpha", std::nullopt, {})});
    f.build();

    const auto result = f.vm->setScenarioHash("e1", "hb");

    EXPECT_TRUE(result.value("ok").toBool());
    ASSERT_EQ(f.uc->setScenarioHashArgs.size(), 1u);
    EXPECT_EQ(f.uc->setScenarioHashArgs[0].first, "e1");
    ASSERT_TRUE(f.uc->setScenarioHashArgs[0].second.has_value());
    EXPECT_EQ(*f.uc->setScenarioHashArgs[0].second, "hb");
}

TEST(BenchmarkManagerResolution, AnEmptyHashForwardsAClearedMapping) {
    Fixture f;
    f.uc->stateValue.draft = draftWith("Saved", {},
                                       {benchmarkEntry("e1", "Alpha", std::string{"h-old"}, {})});
    f.build();

    EXPECT_TRUE(f.vm->setScenarioHash("e1", "").value("ok").toBool());

    ASSERT_EQ(f.uc->setScenarioHashArgs.size(), 1u);
    EXPECT_EQ(f.uc->setScenarioHashArgs[0].first, "e1");
    EXPECT_FALSE(f.uc->setScenarioHashArgs[0].second.has_value());
}

TEST(BenchmarkManagerResolution, SetScenarioHashReportsAnUnknownEntry) {
    Fixture f;
    f.uc->scriptedDraftResults["setScenarioHash"] = {BenchmarkDraftError::UnknownEntry};
    f.build();

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
    f.uc->stateValue.draft = benchmark;
    f.uc->stateValue.draftResolutions = {resolvedEntry("d1", "hn")};
    f.build();

    const auto *categoryNode = groupChild(f.vm->root(), 0);
    EXPECT_EQ(scenarioChild(categoryNode, 0)->matchState(), QStringLiteral("resolved"));
}
