#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QLocale>
#include <QDateTime>
#include <QSet>
#include <QSignalSpy>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
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

    using TM = BenchmarkTableModel;
    constexpr int ScenarioColumn = static_cast<int>(BenchmarkColumnKind::Scenario);

    const TM &tableOf(const BenchmarkManagerViewModel &vm) { return *qobject_cast<const TM *>(vm.tableModel()); }

    // Reads the entry's row; `column` defaults to the Scenario column.
    QVariant entryData(const BenchmarkManagerViewModel &vm, const QString &entryId, int role,
                       int column = ScenarioColumn) {
        const auto &model = tableOf(vm);
        return model.data(model.index(model.rowForEntry(entryId), column), role);
    }

    QVariant thresholdData(const BenchmarkManagerViewModel &vm, const QString &entryId, const QString &tierId,
                           int role) {
        return entryData(vm, entryId, role, tableOf(vm).columnForTier(tierId));
    }

    // Row order: categories in order, Uncategorized last.
    QStringList scenarioNames(const BenchmarkManagerViewModel &vm) {
        const auto &model = tableOf(vm);
        QStringList names;
        for (int row = 0; row < model.rowCount(); ++row)
            names.push_back(model.data(model.index(row, ScenarioColumn), TM::DisplayTextRole).toString());
        return names;
    }

    QVariantMap groupAt(const BenchmarkManagerViewModel &vm, qsizetype index) {
        return vm.groups().at(index).toMap();
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
    EXPECT_EQ(scenarioNames(*f.vm), (QStringList{"a", "b", "c"}));
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
    EXPECT_EQ(f.vm->tableModel()->rowCount(), 0);
    EXPECT_EQ(f.vm->groups().size(), 0);
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

    const auto *model = f.vm->tableModel();
    ASSERT_EQ(model->rowCount(), 1);
    EXPECT_EQ(model->data(model->index(0, ScenarioColumn), TM::EntryIdRole).toString(), "e1");
    ASSERT_EQ(model->columnCount(), TM::FixedColumnCount + 1);
    const int goldColumn = tableOf(*f.vm).columnForTier("gold");
    ASSERT_EQ(goldColumn, TM::FixedColumnCount);
    EXPECT_EQ(model->headerData(goldColumn, Qt::Horizontal, TM::DisplayTextRole).toString(), "Gold");
    EXPECT_EQ(thresholdData(*f.vm, "e1", "gold", TM::TierIdRole).toString(), "gold");
    EXPECT_EQ(thresholdData(*f.vm, "e1", "gold", TM::EditTextRole).toString(),
              BenchmarkCellInput::editText(100.0, QLocale()));
    EXPECT_TRUE(thresholdData(*f.vm, "e1", "gold", TM::HasValueRole).toBool());
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
    EXPECT_EQ(scenarioNames(*f.vm), (QStringList{"S", "S2"}));  // the empty Clicking category has no row
    ASSERT_EQ(f.vm->groups().size(), 1);
    EXPECT_EQ(groupAt(*f.vm, 0)["name"].toString(), "Clicking");
    EXPECT_EQ(thresholdData(*f.vm, "e1", "gold", TM::EditTextRole).toString(),
              BenchmarkCellInput::editText(250.0, QLocale()));
    EXPECT_TRUE(thresholdData(*f.vm, "e1", "gold", TM::HasValueRole).toBool());
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

    ASSERT_EQ(f.vm->groups().size(), 3);
    EXPECT_EQ(groupAt(*f.vm, 0)["name"].toString(), "Clicking");
    EXPECT_EQ(groupAt(*f.vm, 0)["kind"].toString(), "category");
    EXPECT_EQ(groupAt(*f.vm, 1)["id"].toString(), "h1");
    EXPECT_EQ(groupAt(*f.vm, 1)["parentId"].toString(), "c1");
    EXPECT_EQ(groupAt(*f.vm, 1)["scenarioCount"].toInt(), 1);
    EXPECT_EQ(groupAt(*f.vm, 2)["id"].toString(), "s1");
    EXPECT_EQ(groupAt(*f.vm, 2)["scenarioCount"].toInt(), 0);
    EXPECT_EQ(scenarioNames(*f.vm), QStringList{"S"});
    EXPECT_EQ(entryData(*f.vm, "e1", TM::CategoryIdRole).toString(), "c1");
    EXPECT_EQ(entryData(*f.vm, "e1", TM::SubcategoryIdRole).toString(), "h1");

    EXPECT_TRUE(f.vm->addSubcategory("c1", "Dynamic")["ok"].toBool());
    ASSERT_EQ(f.vm->groups().size(), 4);
    EXPECT_EQ(groupAt(*f.vm, 3)["name"].toString(), "Dynamic");
    EXPECT_EQ(groupAt(*f.vm, 3)["parentId"].toString(), "c1");
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
    EXPECT_EQ(scenarioNames(*f.vm), QStringList{"S"});
    EXPECT_EQ(entryData(*f.vm, "e1", TM::CategoryIdRole).toString(), "c1");
    ASSERT_EQ(f.vm->groups().size(), 1);
    EXPECT_EQ(groupAt(*f.vm, 0)["scenarioCount"].toInt(), 1);

    EXPECT_TRUE(f.vm->moveScenario("e1", QString())["ok"].toBool());  // empty target = uncategorized
    EXPECT_EQ(scenarioNames(*f.vm), QStringList{"S"});
    EXPECT_EQ(entryData(*f.vm, "e1", TM::CategoryIdRole).toString(), QString());
    ASSERT_EQ(f.vm->groups().size(), 1);  // the emptied category survives
    EXPECT_EQ(groupAt(*f.vm, 0)["scenarioCount"].toInt(), 0);
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
    EXPECT_EQ(scenarioNames(*f.vm), QStringList{"New"});
    EXPECT_EQ(entryData(*f.vm, "e1", TM::MappingStateRole).toString(), QStringLiteral("resolved"));
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

    EXPECT_EQ(entryData(*f.vm, "e1", TM::MappingStateRole).toString(), QStringLiteral("resolved"));
    EXPECT_EQ(entryData(*f.vm, "e2", TM::MappingStateRole).toString(), QStringLiteral("unresolved"));
    EXPECT_EQ(entryData(*f.vm, "e3", TM::MappingStateRole).toString(), QStringLiteral("mappedUnavailable"));
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

    EXPECT_EQ(entryData(*f.vm, "e1", TM::MappingStateRole).toString(), QStringLiteral("ambiguous"));
    const auto candidates = entryData(*f.vm, "e1", TM::MappingCandidatesRole).toList();
    ASSERT_EQ(candidates.size(), 2);
    const auto candidate = candidates.at(0).toMap();
    EXPECT_EQ(candidate.value("hash").toString(), QStringLiteral("ha"));
    EXPECT_EQ(candidate.value("runCount").toInt(), 3);
    EXPECT_EQ(candidate.value("lastPlayed").toDateTime().toSecsSinceEpoch(), 1'700'000'000);
    EXPECT_FALSE(candidates.at(1).toMap().value("lastPlayed").toDateTime().isValid());
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

    EXPECT_EQ(entryData(*f.vm, "e1", TM::MappingStateRole).toString(), QStringLiteral("autoMappable"));
    const auto candidates = entryData(*f.vm, "e1", TM::MappingCandidatesRole).toList();
    ASSERT_EQ(candidates.size(), 1);
    EXPECT_EQ(candidates.at(0).toMap().value("hash").toString(), QStringLiteral("h1"));
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
    ASSERT_FALSE(f.uc->resolveCalls.empty());
    EXPECT_EQ(f.uc->resolveCalls.back().uncategorized.at(0).hash, std::optional<std::string>("hb"));
    EXPECT_TRUE(f.vm->dirty());
    expectNoMutationForwarded(*f.uc);
}

TEST(BenchmarkManagerResolution, SetScenarioHashEmptyClearsLocalMapping) {
    Fixture f;
    f.uc->nextNewSeed = newSeed(draftWith("Saved", {},
                                          {benchmarkEntry("e1", "Alpha", std::string{"h-old"}, {})}));
    f.build();
    f.vm->beginNewBenchmark();
    ASSERT_FALSE(f.uc->resolveCalls.empty());
    ASSERT_TRUE(f.uc->resolveCalls.back().uncategorized.at(0).hash.has_value());

    EXPECT_TRUE(f.vm->setScenarioHash("e1", "").value("ok").toBool());

    EXPECT_FALSE(f.uc->resolveCalls.back().uncategorized.at(0).hash.has_value());
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

    EXPECT_EQ(entryData(*f.vm, "d1", TM::CategoryIdRole).toString(), "c1");
    EXPECT_EQ(entryData(*f.vm, "d1", TM::MappingStateRole).toString(), QStringLiteral("resolved"));
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

// ── Table projection ────────────────────────────────────────────────────────────────────────────

namespace {
    ScenarioEntry tableEntry(const std::string &id, std::optional<std::string> hash = std::nullopt,
                             const std::vector<std::pair<std::string, double>> &scores = {}) {
        return benchmarkEntry(id, "Scenario " + id, std::move(hash), scores);
    }

    Category tableCategory(const std::string &id, std::vector<ScenarioEntry> direct,
                           std::vector<Subcategory> subs = {}) {
        return Category{GroupId{id}, "Category " + id, BenchmarkColor{10, 20, 30, 255}, std::move(direct),
                        std::move(subs)};
    }

    Subcategory tableSub(const std::string &id, std::vector<ScenarioEntry> entries) {
        return Subcategory{GroupId{id}, "Sub " + id, BenchmarkColor{40, 50, 60, 255}, std::move(entries)};
    }

    // Categories g1 (direct a,b) and g2 (p: c,d; q: e); Uncategorized u; ladder t2,t1.
    Benchmark projectionBench() {
        Benchmark bench;
        bench.id = BenchmarkId{"b1"};
        bench.name = "Bench";
        bench.tiers = {Tier{TierId{"t2"}, "Second", BenchmarkColor{1, 2, 3, 255}},
                       Tier{TierId{"t1"}, "First", BenchmarkColor{4, 5, 6, 255}}};
        bench.categories = {
            tableCategory("g1", {tableEntry("a", "ha", {{"t2", 1}, {"t1", 2}}), tableEntry("b")}),
            tableCategory("g2", {}, {tableSub("p", {tableEntry("c"), tableEntry("d")}),
                                     tableSub("q", {tableEntry("e")})}),
        };
        bench.uncategorized = {tableEntry("u")};
        return bench;
    }

    struct TableFixture : Fixture {
        QAbstractItemModel *model = nullptr;

        void open(Benchmark bench) {
            uc->nextOpenSeed = librarySeed(std::move(bench));
            build();
            ASSERT_TRUE(vm->openBenchmark(QStringLiteral("b1")));
            model = vm->tableModel();
        }

        [[nodiscard]] QVariant cell(int row, int column, int role) const {
            return model->data(model->index(row, column), role);
        }

        [[nodiscard]] QStringList rowIds() const {
            QStringList ids;
            for (int row = 0; row < model->rowCount(); ++row) ids.push_back(cell(row, 2, TM::EntryIdRole).toString());
            return ids;
        }

        [[nodiscard]] int rowOf(const QString &entryId) const {
            int row = -1;
            QMetaObject::invokeMethod(model, "rowForEntry", Q_RETURN_ARG(int, row), Q_ARG(QString, entryId));
            return row;
        }

        [[nodiscard]] int columnOf(const QString &tierId) const {
            int column = -1;
            QMetaObject::invokeMethod(model, "columnForTier", Q_RETURN_ARG(int, column), Q_ARG(QString, tierId));
            return column;
        }
    };
}

TEST(BenchmarkTableProjection, HierarchyRowsAndSpans) {
    TableFixture f;
    f.uc->nextResolveResult = {resolvedEntry("a", "ha"), unresolvedEntry("b")};
    auto bench = projectionBench();
    bench.categories.at(0).name = " ";
    f.open(bench);

    ASSERT_EQ(f.rowIds(), (QStringList{"a", "b", "c", "d", "e", "u"}));
    ASSERT_EQ(f.model->columnCount(), 5);
    const QList<int> kinds{static_cast<int>(BenchmarkColumnKind::Category),
                           static_cast<int>(BenchmarkColumnKind::Subcategory),
                           static_cast<int>(BenchmarkColumnKind::Scenario),
                           static_cast<int>(BenchmarkColumnKind::Threshold),
                           static_cast<int>(BenchmarkColumnKind::Threshold)};
    for (int column = 0; column < 5; ++column) {
        EXPECT_EQ(f.cell(0, column, TM::ColumnKindRole).toInt(), kinds.at(column)) << column;
        EXPECT_EQ(f.model->headerData(column, Qt::Horizontal, TM::ColumnKindRole).toInt(), kinds.at(column));
    }
    EXPECT_EQ(f.model->headerData(3, Qt::Horizontal, TM::TierIdRole).toString(), "t2");
    EXPECT_EQ(f.model->headerData(3, Qt::Horizontal, Qt::DisplayRole).toString(), "Second");
    EXPECT_EQ(f.model->headerData(4, Qt::Horizontal, TM::TierIdRole).toString(), "t1");
    EXPECT_EQ(f.cell(0, 3, TM::TierIdRole).toString(), "t2");
    EXPECT_EQ(f.columnOf("t1"), 4);
    EXPECT_EQ(f.rowOf("e"), 4);
    EXPECT_EQ(f.rowOf("missing"), -1);

    // Category spans: g1 rows 0-1, g2 rows 2-4; subcategory spans: p rows 2-3, q row 4.
    const auto span = [&](int row, int startRole, int lengthRole) {
        return std::pair{f.cell(row, 0, startRole).toInt(), f.cell(row, 0, lengthRole).toInt()};
    };
    EXPECT_EQ(span(1, TM::CategorySpanStartRole, TM::CategorySpanLengthRole), std::pair(0, 2));
    EXPECT_EQ(span(3, TM::CategorySpanStartRole, TM::CategorySpanLengthRole), std::pair(2, 3));
    EXPECT_EQ(span(3, TM::SubcategorySpanStartRole, TM::SubcategorySpanLengthRole), std::pair(2, 2));
    EXPECT_EQ(span(4, TM::SubcategorySpanStartRole, TM::SubcategorySpanLengthRole), std::pair(4, 1));
    EXPECT_EQ(f.cell(0, 1, TM::SubcategoryIdRole).toString(), QString());
    EXPECT_EQ(f.cell(0, 0, TM::CategoryIdRole).toString(), "g1");
    EXPECT_EQ(f.cell(2, 1, TM::SubcategoryIdRole).toString(), "p");
    EXPECT_EQ(f.cell(2, 1, TM::SubcategoryNameRole).toString(), "Sub p");
    EXPECT_EQ(f.cell(2, 0, TM::CategoryColorRole).value<QColor>(), QColor(10, 20, 30));
    EXPECT_EQ(f.cell(4, 1, TM::SubcategoryColorRole).value<QColor>(), QColor(40, 50, 60));

    // Uncategorized is neutral: no group id or colour, but a readable label.
    EXPECT_EQ(f.cell(5, 0, TM::CategoryIdRole).toString(), QString());
    EXPECT_FALSE(f.cell(5, 0, TM::CategoryColorRole).value<QColor>().isValid());
    EXPECT_FALSE(f.cell(5, 0, TM::CategoryNameRole).toString().isEmpty());
    EXPECT_EQ(span(5, TM::CategorySpanStartRole, TM::CategorySpanLengthRole), std::pair(5, 1));

    EXPECT_EQ(f.cell(0, 2, TM::DisplayTextRole).toString(), "Scenario a");
    EXPECT_TRUE(f.cell(0, 3, TM::HasValueRole).toBool());
    EXPECT_FALSE(f.cell(1, 3, TM::HasValueRole).toBool());
    EXPECT_EQ(f.cell(0, 2, TM::MappingStateRole).toString(), "resolved");
    EXPECT_EQ(f.cell(1, 2, TM::MappingStateRole).toString(), "unresolved");

    // Issues keep their natural scope: a missing b/t1 threshold lands on that cell only, and the
    // blank category name on the category.
    EXPECT_FALSE(f.cell(1, 4, TM::IssuesRole).toStringList().isEmpty());
    EXPECT_TRUE(f.cell(0, 4, TM::IssuesRole).toStringList().isEmpty());
    EXPECT_FALSE(f.cell(0, 0, TM::IssuesRole).toStringList().isEmpty());
    EXPECT_TRUE(f.cell(1, 2, TM::IssuesRole).toStringList().isEmpty());

    // Edits keep the projection coherent.
    ASSERT_TRUE(f.vm->renameScenario("b", "Renamed")["ok"].toBool());
    EXPECT_EQ(f.cell(1, 2, TM::DisplayTextRole).toString(), "Renamed");
}

TEST(BenchmarkTableProjection, EmptyAndMixedGroupsRemainRepairable) {
    TableFixture f;
    auto bench = projectionBench();
    bench.categories.push_back(tableCategory("empty", {}));
    bench.categories.at(1).subcategories.push_back(tableSub("hollow", {}));
    // g1 is loaded mixed: direct a,b plus a subcategory holding m.
    bench.categories.at(0).subcategories.push_back(tableSub("m-sub", {tableEntry("m")}));
    f.open(bench);

    EXPECT_EQ(f.rowIds(), (QStringList{"a", "b", "m", "c", "d", "e", "u"}));
    EXPECT_EQ(f.cell(2, 0, TM::CategorySpanLengthRole).toInt(), 3);
    EXPECT_EQ(f.cell(2, 1, TM::SubcategoryIdRole).toString(), "m-sub");
    EXPECT_FALSE(f.cell(0, 0, TM::IssuesRole).toStringList().isEmpty());

    const auto groups = f.vm->property("groups").toList();
    const auto find = [&](const QString &id) {
        for (const auto &group: groups)
            if (group.toMap()["id"].toString() == id) return group.toMap();
        return QVariantMap{};
    };
    const auto empty = find("empty");
    ASSERT_FALSE(empty.isEmpty());
    EXPECT_EQ(empty["kind"].toString(), "category");
    EXPECT_EQ(empty["scenarioCount"].toInt(), 0);
    EXPECT_FALSE(empty["issues"].toStringList().isEmpty());
    const auto hollow = find("hollow");
    ASSERT_FALSE(hollow.isEmpty());
    EXPECT_EQ(hollow["kind"].toString(), "subcategory");
    EXPECT_EQ(hollow["parentId"].toString(), "g2");
    EXPECT_FALSE(hollow["issues"].toStringList().isEmpty());
    EXPECT_FALSE(find("g1")["issues"].toStringList().isEmpty());
}

// ── Threshold input ─────────────────────────────────────────────────────────────────────────────

namespace {
    class DefaultLocaleGuard {
    public:
        explicit DefaultLocaleGuard(const QLocale &locale) { QLocale::setDefault(locale); }
        ~DefaultLocaleGuard() { QLocale::setDefault(m_previous); }
        DefaultLocaleGuard(const DefaultLocaleGuard &) = delete;
        DefaultLocaleGuard &operator=(const DefaultLocaleGuard &) = delete;

    private:
        QLocale m_previous;
    };

    // a has t2=1 and t1=1234.5678901234567; b has no thresholds.
    Benchmark inputBench() {
        auto bench = projectionBench();
        bench.categories.at(0).scenarios.at(0).thresholds = {{TierId{"t2"}, 1.0}, {TierId{"t1"}, 1234.5678901234567}};
        return bench;
    }
}

TEST(BenchmarkTableInput, GroupedScientificAndPrecisionInSession) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(inputBench());
    const int a = f.rowOf("a");
    const int b = f.rowOf("b");
    const int t1 = f.columnOf("t1");

    EXPECT_EQ(f.cell(a, t1, TM::DisplayTextRole).toString(), "1,234.5678901234567");
    const auto edit = f.cell(a, t1, TM::EditTextRole).toString();
    EXPECT_EQ(edit, "1234.5678901234567");

    // Activating and leaving an unchanged cell commits its edit text back: no rounding, not dirty.
    ASSERT_TRUE(f.vm->editThresholdText("a", "t1", edit)["ok"].toBool());
    EXPECT_FALSE(f.vm->dirty());
    ASSERT_TRUE(f.vm->editThresholdText("a", "t1", f.cell(a, t1, TM::DisplayTextRole).toString())["ok"].toBool());
    EXPECT_FALSE(f.vm->dirty());

    ASSERT_TRUE(f.vm->editThresholdText("b", "t1", " 1,000.5 ")["ok"].toBool());
    EXPECT_TRUE(f.cell(b, t1, TM::HasValueRole).toBool());
    EXPECT_EQ(f.cell(b, t1, TM::DisplayTextRole).toString(), "1,000.5");
    EXPECT_EQ(f.cell(b, t1, TM::InputStateRole).toString(), QString());
    EXPECT_TRUE(f.vm->dirty());
    ASSERT_TRUE(f.vm->editThresholdText("b", "t2", "2.5e2")["ok"].toBool());
    EXPECT_EQ(f.cell(b, f.columnOf("t2"), TM::DisplayTextRole).toString(), "250");

    EXPECT_FALSE(f.vm->editThresholdText("missing", "t1", "1")["ok"].toBool());
    EXPECT_FALSE(f.vm->editThresholdText("a", "missing", "1")["ok"].toBool());
}

TEST(BenchmarkTableInput, MalformedAndNonFiniteRemainExactInSession) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(projectionBench());
    const int b = f.rowOf("b");
    const int t1 = f.columnOf("t1");
    const auto missingText = benchmarkIssueText(BenchmarkIssueCode::MissingThreshold);
    ASSERT_TRUE(f.cell(b, t1, TM::IssuesRole).toStringList().contains(missingText));

    const std::vector<std::pair<QString, QString>> inputs{
        {"12oops", "invalid"}, {"=1+2", "invalid"}, {"1,23,4", "invalid"},
        {" NaN", "nonFinite"}, {"infinity", "nonFinite"}, {"1e9999", "nonFinite"}};
    for (const auto &[text, state]: inputs) {
        ASSERT_TRUE(f.vm->editThresholdText("b", "t1", text)["ok"].toBool()) << text.toStdString();
        EXPECT_EQ(f.cell(b, t1, TM::DisplayTextRole).toString(), text);
        EXPECT_EQ(f.cell(b, t1, TM::EditTextRole).toString(), text);
        EXPECT_EQ(f.cell(b, t1, TM::InputStateRole).toString(), state);
        EXPECT_FALSE(f.cell(b, t1, TM::HasValueRole).toBool());
        const auto issues = f.cell(b, t1, TM::IssuesRole).toStringList();
        ASSERT_FALSE(issues.isEmpty());
        EXPECT_FALSE(issues.contains(missingText)) << "input issue supersedes the missing label";
        EXPECT_TRUE(f.vm->dirty()) << "retained input into an already missing cell is unsaved work";
    }
    const auto nonFiniteIssue = f.cell(b, t1, TM::IssuesRole).toStringList().front();
    f.vm->editThresholdText("b", "t1", "12oops");
    EXPECT_NE(f.cell(b, t1, TM::IssuesRole).toStringList().front(), nonFiniteIssue)
        << "non-finite and non-numeric input explain themselves differently";

    // The raw text is keyed by IDs, so it survives an unrelated edit that rebuilds the table.
    ASSERT_TRUE(f.vm->renameScenario("a", "Renamed")["ok"].toBool());
    EXPECT_EQ(f.cell(b, t1, TM::DisplayTextRole).toString(), "12oops");

    ASSERT_TRUE(f.vm->editThresholdText("b", "t1", "42")["ok"].toBool());
    EXPECT_EQ(f.cell(b, t1, TM::InputStateRole).toString(), QString());
    EXPECT_TRUE(f.cell(b, t1, TM::HasValueRole).toBool());
    EXPECT_EQ(f.cell(b, t1, TM::DisplayTextRole).toString(), "42");

    ASSERT_TRUE(f.vm->editThresholdText("b", "t1", "oops")["ok"].toBool());
    ASSERT_TRUE(f.vm->editThresholdText("b", "t1", "  ")["ok"].toBool());
    EXPECT_FALSE(f.cell(b, t1, TM::HasValueRole).toBool());
    EXPECT_EQ(f.cell(b, t1, TM::InputStateRole).toString(), QString());
    EXPECT_TRUE(f.cell(b, t1, TM::IssuesRole).toStringList().contains(missingText));
    ASSERT_TRUE(f.vm->renameScenario("a", "Scenario a")["ok"].toBool());
    EXPECT_FALSE(f.vm->dirty());

    // Zero and finite negatives stay typed values; negatives keep their domain issue.
    ASSERT_TRUE(f.vm->editThresholdText("b", "t2", "0")["ok"].toBool());
    EXPECT_TRUE(f.cell(b, f.columnOf("t2"), TM::HasValueRole).toBool());
    ASSERT_TRUE(f.vm->editThresholdText("b", "t2", "-5")["ok"].toBool());
    EXPECT_TRUE(f.cell(b, f.columnOf("t2"), TM::IssuesRole).toStringList().contains(
        benchmarkIssueText(BenchmarkIssueCode::NegativeThreshold)));
}

// ── Paste ───────────────────────────────────────────────────────────────────────────────────────

namespace {
    // g1 holds a (mapped ha) and b (mapped hb); Uncategorized holds u. Ladder t1, t2.
    Benchmark pasteBench() {
        Benchmark bench;
        bench.id = BenchmarkId{"b1"};
        bench.name = "Bench";
        bench.tiers = {Tier{TierId{"t1"}, "One", {}}, Tier{TierId{"t2"}, "Two", {}}};
        bench.categories = {tableCategory("g1", {benchmarkEntry("a", "A", "ha", {{"t1", 10}, {"t2", 20}}),
                                                 benchmarkEntry("b", "B", "hb", {{"t1", 11}, {"t2", 21}})})};
        bench.uncategorized = {benchmarkEntry("u", "U", std::nullopt, {{"t1", 12}, {"t2", 22}})};
        return bench;
    }

    QVariantMap at(const QString &kind, const QString &entryId = {}, const QString &tierId = {}) {
        return QVariantMap{{"kind", kind}, {"entryId", entryId}, {"tierId", tierId}};
    }

    // The working copy as the view model last handed it to resolution: every edit re-resolves.
    const Benchmark &working(const TableFixture &f) { return f.uc->resolveCalls.back(); }

    const ScenarioEntry &entryIn(const Benchmark &bench, const std::string &id) {
        const auto matches = [&](const ScenarioEntry &e) { return e.id.value == id; };
        for (const auto &e: bench.uncategorized) if (matches(e)) return e;
        for (const auto &category: bench.categories) {
            for (const auto &e: category.scenarios) if (matches(e)) return e;
            for (const auto &sub: category.subcategories) for (const auto &e: sub.scenarios) if (matches(e)) return e;
        }
        throw std::out_of_range("no entry " + id);
    }

    std::optional<double> scoreOf(const Benchmark &bench, const std::string &entry, const std::string &tier) {
        for (const auto &threshold: entryIn(bench, entry).thresholds)
            if (threshold.tierId.value == tier) return threshold.score;
        return std::nullopt;
    }
}

TEST(BenchmarkTablePaste, SeparatePassesAndUntouchedNeighbours) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    {
        TableFixture f;
        f.open(pasteBench());
        const auto resolvesBefore = f.uc->resolveCalls.size();
        QSignalSpy changed(f.vm.get(), &BenchmarkManagerViewModel::draftChanged);

        ASSERT_TRUE(f.vm->pasteText(at("scenario", "a"), "Alpha\nBeta\n")["ok"].toBool());

        EXPECT_EQ(f.uc->resolveCalls.size(), resolvesBefore + 1);
        EXPECT_EQ(changed.count(), 1);
        const auto &bench = working(f);
        EXPECT_EQ(entryIn(bench, "a").name, "Alpha");
        EXPECT_EQ(entryIn(bench, "b").name, "Beta");
        EXPECT_EQ(entryIn(bench, "a").hash, std::optional<std::string>("ha"));
        EXPECT_EQ(entryIn(bench, "b").hash, std::optional<std::string>("hb"));
        EXPECT_EQ(bench.categories.at(0).scenarios.size(), 2U);
        EXPECT_EQ(entryIn(bench, "u"), pasteBench().uncategorized.at(0));
        EXPECT_EQ(scoreOf(bench, "a", "t1"), 10.0);
    }
    {
        TableFixture f;
        f.open(pasteBench());
        ASSERT_TRUE(f.vm->pasteText(at("threshold", "a", "t2"), "30\n\n0\n")["ok"].toBool());
        const auto &bench = working(f);
        EXPECT_EQ(scoreOf(bench, "a", "t2"), 30.0);
        EXPECT_EQ(scoreOf(bench, "b", "t2"), std::nullopt) << "a blank cell clears";
        EXPECT_EQ(scoreOf(bench, "u", "t2"), 0.0) << "zero is a score";
        EXPECT_EQ(scoreOf(bench, "a", "t1"), 10.0);
        EXPECT_EQ(scoreOf(bench, "b", "t1"), 11.0);
        EXPECT_EQ(scoreOf(bench, "u", "t1"), 12.0);
    }
    {
        TableFixture f;
        f.open(pasteBench());
        ASSERT_TRUE(f.vm->pasteText(at("threshold", "b", "t1"), "5\toops\n6\t7")["ok"].toBool());
        const auto &bench = working(f);
        EXPECT_EQ(scoreOf(bench, "b", "t1"), 5.0);
        EXPECT_EQ(scoreOf(bench, "b", "t2"), std::nullopt);
        EXPECT_EQ(f.cell(f.rowOf("b"), f.columnOf("t2"), TM::DisplayTextRole).toString(), "oops");
        EXPECT_EQ(f.cell(f.rowOf("b"), f.columnOf("t2"), TM::InputStateRole).toString(), "invalid");
        EXPECT_EQ(scoreOf(bench, "u", "t1"), 6.0);
        EXPECT_EQ(scoreOf(bench, "u", "t2"), 7.0);
        EXPECT_EQ(entryIn(bench, "a"), pasteBench().categories.at(0).scenarios.at(0));
    }
    {
        TableFixture f;
        f.open(pasteBench());
        ASSERT_TRUE(f.vm->pasteText(at("scenario", "u"), "Named\t1\t2")["ok"].toBool());
        const auto &bench = working(f);
        EXPECT_EQ(entryIn(bench, "u").name, "Named");
        EXPECT_EQ(scoreOf(bench, "u", "t1"), 1.0);
        EXPECT_EQ(scoreOf(bench, "u", "t2"), 2.0);
    }
}

TEST(BenchmarkTablePaste, ExpansionRetainsCompleteBlock) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    {
        TableFixture f;
        Benchmark bench = pasteBench();
        bench.categories.clear();
        bench.tiers.resize(1);
        bench.uncategorized.at(0).thresholds.resize(1);
        f.open(bench);

        ASSERT_TRUE(f.vm->pasteText(at("threshold", "u", "t1"), "1\t2\t3\nx\t\t5\n7\tNaN\t9")["ok"].toBool());

        const auto &result = working(f);
        ASSERT_EQ(result.tiers.size(), 3U);
        ASSERT_EQ(result.uncategorized.size(), 3U);
        EXPECT_EQ(result.uncategorized.at(0).id.value, "u");
        EXPECT_NE(result.tiers.at(1).color, result.tiers.at(2).color);
        for (std::size_t i = 1; i < 3; ++i) {
            EXPECT_TRUE(result.tiers.at(i).name.empty());
            EXPECT_TRUE(result.uncategorized.at(i).name.empty());
            EXPECT_FALSE(result.uncategorized.at(i).hash.has_value());
        }
        const auto tier = [&](std::size_t i) { return result.tiers.at(i).id.value; };
        const auto row = [&](std::size_t i) { return result.uncategorized.at(i).id.value; };
        EXPECT_EQ(scoreOf(result, row(0), tier(0)), 1.0);
        EXPECT_EQ(scoreOf(result, row(0), tier(2)), 3.0);
        EXPECT_EQ(scoreOf(result, row(1), tier(0)), std::nullopt);
        EXPECT_EQ(f.cell(1, 3, TM::DisplayTextRole).toString(), "x");
        EXPECT_EQ(scoreOf(result, row(1), tier(1)), std::nullopt);
        EXPECT_EQ(f.cell(1, 4, TM::InputStateRole).toString(), QString());
        EXPECT_EQ(scoreOf(result, row(1), tier(2)), 5.0);
        EXPECT_EQ(scoreOf(result, row(2), tier(0)), 7.0);
        EXPECT_EQ(f.cell(2, 4, TM::InputStateRole).toString(), "nonFinite");
        EXPECT_EQ(scoreOf(result, row(2), tier(2)), 9.0);
        EXPECT_EQ(f.model->rowCount(), 3);
        EXPECT_EQ(f.model->columnCount(), 6);
    }
    {
        TableFixture f;
        f.uc->nextNewSeed = newSeed(draftWith("", {}, {}));
        f.build();
        f.vm->beginNewBenchmark();

        ASSERT_TRUE(f.vm->pasteText(at("append"), "S1\t10\nS2\t20\n")["ok"].toBool());

        const auto &result = working(f);
        ASSERT_EQ(result.uncategorized.size(), 2U);
        ASSERT_EQ(result.tiers.size(), 1U);
        EXPECT_EQ(result.uncategorized.at(0).name, "S1");
        EXPECT_EQ(result.uncategorized.at(1).name, "S2");
        EXPECT_EQ(scoreOf(result, result.uncategorized.at(1).id.value, result.tiers.at(0).id.value), 20.0);
    }
    {
        TableFixture f;
        Benchmark bench = pasteBench();
        bench.tiers.resize(1);
        f.open(bench);

        ASSERT_TRUE(f.vm->pasteText(at("rankHeader", {}, "t1"), "Bronze\tSilver\tGold\n")["ok"].toBool());

        const auto &result = working(f);
        ASSERT_EQ(result.tiers.size(), 3U);
        EXPECT_EQ(result.tiers.at(0).id.value, "t1");
        EXPECT_EQ(result.tiers.at(0).name, "Bronze");
        EXPECT_EQ(result.tiers.at(1).name, "Silver");
        EXPECT_EQ(result.tiers.at(2).name, "Gold");
        EXPECT_EQ(f.model->rowCount(), 3);
    }
    {
        TableFixture f;
        f.uc->nextNewSeed = newSeed(draftWith("", {}, {}));
        f.build();
        f.vm->beginNewBenchmark();
        ASSERT_TRUE(f.vm->pasteText(at("rankHeader"), "A\tB")["ok"].toBool());
        ASSERT_EQ(working(f).tiers.size(), 2U);
        EXPECT_EQ(working(f).tiers.at(1).name, "B");
    }
}

TEST(BenchmarkTablePaste, PlacementAndStructureRejectAtomically) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(pasteBench());
    ASSERT_TRUE(f.vm->editThresholdText("a", "t1", "oops")["ok"].toBool());
    const Benchmark before = working(f);
    const auto resolves = f.uc->resolveCalls.size();

    const std::vector<std::pair<QVariantMap, QString>> rejected{
        {QVariantMap{{"kind", "category"}, {"groupId", "g1"}}, "1"},
        {QVariantMap{{"kind", "subcategory"}, {"groupId", "g1"}}, "1"},
        {at("rankHeader", {}, "t1"), "A\tB\nC\tD"},
        {at("threshold", "missing", "t1"), "1"},
        {at("threshold", "a", "missing"), "1"},
        {at("scenario", "missing"), "1"},
        {QVariantMap{{"kind", "nonsense"}}, "1"},
    };
    for (const auto &[destination, text]: rejected) {
        const auto result = f.vm->pasteText(destination, text);
        EXPECT_FALSE(result["ok"].toBool()) << destination["kind"].toString().toStdString();
        EXPECT_FALSE(result["error"].toString().isEmpty());
        EXPECT_EQ(f.uc->resolveCalls.size(), resolves);
        EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "oops");
        EXPECT_EQ(f.model->rowCount(), 3);
        EXPECT_EQ(f.model->columnCount(), 5);
    }
    EXPECT_EQ(working(f), before);

    // An empty clipboard is a defined no-op.
    const auto empty = f.vm->pasteText(at("threshold", "a", "t1"), QString());
    EXPECT_TRUE(empty["ok"].toBool());
    EXPECT_EQ(f.uc->resolveCalls.size(), resolves);

    // Ragged rows touch only the fields they supply; nothing is padded into a clearing blank.
    ASSERT_TRUE(f.vm->pasteText(at("threshold", "a", "t1"), "1\t2\n3")["ok"].toBool());
    EXPECT_EQ(scoreOf(working(f), "a", "t1"), 1.0);
    EXPECT_EQ(scoreOf(working(f), "a", "t2"), 2.0);
    EXPECT_EQ(scoreOf(working(f), "b", "t1"), 3.0);
    EXPECT_EQ(scoreOf(working(f), "b", "t2"), 21.0);
}

// ── Undo ────────────────────────────────────────────────────────────────────────────────────────

namespace {
    bool canUndo(const BenchmarkManagerViewModel &vm) { return vm.property("canUndo").toBool(); }

    struct SessionState {
        Benchmark draft;
        QString rawAtA;
        QString rawAtU;
        QStringList selection;
    };
}

TEST(BenchmarkTableUndo, InterleavedBatchAndManualUndo) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(pasteBench());
    EXPECT_FALSE(canUndo(*f.vm));

    std::vector<SessionState> states;
    const auto capture = [&] {
        const int t1 = f.columnOf("t1");
        const int t2 = f.columnOf("t2");
        states.push_back({working(f), f.cell(f.rowOf("a"), t1, TM::DisplayTextRole).toString(),
                          f.rowOf("u") >= 0 && t2 >= 0 ? f.cell(f.rowOf("u"), t2, TM::DisplayTextRole).toString()
                                                       : QString(),
                          f.vm->selectedEntryIds()});
    };
    capture();

    ASSERT_TRUE(f.vm->editThresholdText("a", "t1", "keep")["ok"].toBool());
    capture();
    ASSERT_TRUE(f.vm->pasteText(at("threshold", "a", "t1"),
                                "p1\t2\t3\n4\t5\t6\n7\toops\t9\n10\t11\t12")["ok"].toBool());
    ASSERT_EQ(working(f).uncategorized.size(), 2U);
    const auto created = QString::fromStdString(working(f).uncategorized.at(1).id.value);
    capture();
    ASSERT_TRUE(f.vm->renameScenario("b", "Bee")["ok"].toBool());
    capture();
    ASSERT_TRUE(f.vm->setTierColor("t1", QColor(255, 0, 0))["ok"].toBool());
    capture();
    ASSERT_TRUE(f.vm->reorderTier("t2", 0)["ok"].toBool());
    capture();
    ASSERT_TRUE(f.vm->setScenarioHash("u", "hu")["ok"].toBool());
    f.vm->setSelectedEntryIds({"u", created});
    capture();
    ASSERT_TRUE(f.vm->assignScenarios({created, "u"}, "g1")["ok"].toBool());
    f.vm->setSelectedEntryIds({});
    // Retained input follows its IDs when the row moves.
    EXPECT_EQ(f.cell(f.rowOf("u"), f.columnOf("t2"), TM::DisplayTextRole).toString(), "oops");
    EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "p1");

    // Seven committed edits, seven Undo steps, each restoring exactly the preceding state.
    for (auto expected = states.size(); expected-- > 0;) {
        ASSERT_TRUE(canUndo(*f.vm)) << expected;
        ASSERT_TRUE(f.vm->undo()["ok"].toBool()) << expected;
        EXPECT_EQ(working(f), states[expected].draft) << expected;
        EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), states[expected].rawAtA)
            << expected;
        EXPECT_EQ(f.cell(f.rowOf("u"), f.columnOf("t2"), TM::DisplayTextRole).toString(), states[expected].rawAtU)
            << expected;
        EXPECT_EQ(f.vm->selectedEntryIds(), states[expected].selection) << expected;
    }
    EXPECT_FALSE(canUndo(*f.vm));
    EXPECT_FALSE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(f.rowOf(created), -1) << "undoing the paste removed its expansion";
    EXPECT_FALSE(f.vm->dirty());
}

TEST(BenchmarkTableUndo, BoundariesAndPublications) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    const auto openWithInput = [](TableFixture &f) {
        f.open(pasteBench());
        f.vm->setSelectedEntryIds({"a"});
        f.vm->setCurrentCell(QVariantMap{{"entryId", "a"}, {"tierId", "t1"}});
        EXPECT_FALSE(f.vm->dirty()) << "selection is not an edit";
        EXPECT_FALSE(canUndo(*f.vm));
        EXPECT_EQ(f.vm->selectedEntryIds(), QStringList{"a"});
        EXPECT_EQ(f.vm->currentCell()["tierId"].toString(), "t1");
        ASSERT_TRUE(f.vm->editThresholdText("a", "t1", "oops")["ok"].toBool());
        ASSERT_TRUE(canUndo(*f.vm));
    };
    {
        TableFixture f;
        openWithInput(f);
        ASSERT_TRUE(f.vm->renameScenario("b", "Bee")["ok"].toBool());

        // A publication makes the accepted entry stale and updates profile context, but the local
        // session and its history stay.
        f.uc->stateValue.library = loadedSnapshot("b1", "Bench");
        f.uc->stateValue.library->entries.front().digest = "changed";
        f.uc->nextResolveResult = {resolvedEntry("a", "ha")};
        f.publish();
        EXPECT_TRUE(f.vm->baselineStale());
        EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "oops");
        ASSERT_TRUE(canUndo(*f.vm));

        ASSERT_TRUE(f.vm->undo()["ok"].toBool());
        EXPECT_EQ(entryIn(working(f), "b").name, "B");
        EXPECT_EQ(f.cell(f.rowOf("a"), 2, TM::MappingStateRole).toString(), "resolved")
            << "Undo recomputes against the latest profile";
        EXPECT_TRUE(f.vm->baselineStale()) << "Undo never restores accepted-library state";
        EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "oops");
        EXPECT_TRUE(f.vm->dirty());
    }
    {
        TableFixture f;
        openWithInput(f);
        f.uc->nextOpenSeed = std::nullopt;
        EXPECT_FALSE(f.vm->openBenchmark("other"));
        f.uc->nextImport = {};
        f.uc->nextImport.failure = PlaylistImportFailure::MalformedJson;
        EXPECT_FALSE(f.vm->importPlaylist(QUrl::fromLocalFile("C:/x.json"))["ok"].toBool());
        EXPECT_TRUE(canUndo(*f.vm)) << "failed transitions keep the session";
        EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "oops");

        f.uc->nextOpenSeed = librarySeed(pasteBench());
        ASSERT_TRUE(f.vm->openBenchmark("b1"));
        EXPECT_FALSE(canUndo(*f.vm));
        EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::InputStateRole).toString(), QString());
    }
    {
        TableFixture f;
        openWithInput(f);
        f.vm->discard();
        EXPECT_FALSE(canUndo(*f.vm));
        f.uc->nextNewSeed = newSeed(draftWith("", {}, {}));
        f.vm->beginNewBenchmark();
        EXPECT_FALSE(canUndo(*f.vm));
        EXPECT_EQ(f.model->rowCount(), 0);
    }
    {
        TableFixture f;
        openWithInput(f);
        f.uc->nextImport = {};
        f.uc->nextImport.seed = newSeed(pasteBench());
        ASSERT_TRUE(f.vm->importPlaylist(QUrl::fromLocalFile("C:/x.json"))["ok"].toBool());
        EXPECT_FALSE(canUndo(*f.vm));
        EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::InputStateRole).toString(), QString());
    }
    {
        // A never-saved seed has no accepted baseline, so it stays dirty even after Undo returns to it.
        TableFixture f;
        f.uc->nextNewSeed = newSeed(draftWith("", {}, {}));
        f.build();
        f.vm->beginNewBenchmark();
        f.vm->setBenchmarkName("Named");
        ASSERT_TRUE(canUndo(*f.vm));
        ASSERT_TRUE(f.vm->undo()["ok"].toBool());
        EXPECT_EQ(f.vm->benchmarkName(), QString());
        EXPECT_TRUE(f.vm->dirty());
        // Rejected and no-op commands add no history.
        EXPECT_FALSE(f.vm->renameScenario("missing", "x")["ok"].toBool());
        EXPECT_TRUE(f.vm->pasteText(at("append"), QString())["ok"].toBool());
        EXPECT_FALSE(canUndo(*f.vm));
    }
}

// ── Save normalization ──────────────────────────────────────────────────────────────────────────

namespace {
    bool sameToken(const std::optional<BenchmarkEditToken> &actual, const BenchmarkEditToken &expected) {
        return actual && actual->id == expected.id && actual->filename == expected.filename &&
               actual->digest == expected.digest;
    }
}

TEST(BenchmarkTableSave, SuccessfulNormalizationAndBoundary) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(pasteBench());
    const auto originalToken = BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d1"};
    for (const auto &[entry, tier, text]: std::vector<std::tuple<QString, QString, QString>>{
             {"a", "t1", "oops"}, {"a", "t2", "NaN"}, {"b", "t1", "infinity"}, {"b", "t2", "1e9999"},
             {"u", "t1", "0"}, {"u", "t2", "-3"}})
        ASSERT_TRUE(f.vm->editThresholdText(entry, tier, text)["ok"].toBool());
    ASSERT_TRUE(f.vm->pasteText(at("append"), "\t5")["ok"].toBool());
    const auto createdRow = working(f).uncategorized.back().id;
    ASSERT_TRUE(canUndo(*f.vm));
    EXPECT_EQ(f.vm->normalizationIssues().size(), 4);

    const BenchmarkEditToken admitted{BenchmarkId{"b1"}, "b1.json", "d2"};
    f.uc->nextSaveOutcome = {admitted, std::nullopt};
    QString rawDuringSave;
    f.uc->duringSave = [&] {
        // Accepted publication re-enters the view model before save() returns.
        f.uc->stateValue.library = loadedSnapshot("b1", "Bench");
        f.publish();
        rawDuringSave = f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString();
    };

    ASSERT_TRUE(f.vm->save()["ok"].toBool());

    ASSERT_EQ(f.uc->saveCalls.size(), 1U);
    const auto &[submitted, token] = f.uc->saveCalls.front();
    EXPECT_TRUE(sameToken(token, originalToken));
    EXPECT_EQ(rawDuringSave, "oops") << "the visible session is untouched until the outcome";
    for (const auto *entry: {"a", "b"})
        for (const auto *tier: {"t1", "t2"}) EXPECT_EQ(scoreOf(submitted, entry, tier), std::nullopt) << entry << tier;
    EXPECT_EQ(scoreOf(submitted, "u", "t1"), 0.0);
    EXPECT_EQ(scoreOf(submitted, "u", "t2"), -3.0);
    EXPECT_EQ(entryIn(submitted, "a").hash, std::optional<std::string>("ha"));
    EXPECT_EQ(entryIn(submitted, createdRow.value).name, "");
    EXPECT_EQ(scoreOf(submitted, createdRow.value, "t1"), 5.0);

    // Success adopts exactly the submitted candidate and starts a new history boundary.
    EXPECT_EQ(working(f), submitted);
    EXPECT_FALSE(canUndo(*f.vm));
    EXPECT_FALSE(f.vm->dirty());
    EXPECT_TRUE(f.vm->normalizationIssues().isEmpty());
    const int a = f.rowOf("a");
    const int t1 = f.columnOf("t1");
    EXPECT_EQ(f.cell(a, t1, TM::InputStateRole).toString(), QString());
    EXPECT_FALSE(f.cell(a, t1, TM::HasValueRole).toBool());
    EXPECT_TRUE(f.cell(a, t1, TM::IssuesRole).toStringList().contains(
        benchmarkIssueText(BenchmarkIssueCode::MissingThreshold)));
    EXPECT_FALSE(f.vm->draftTrackable());

    f.uc->duringSave = nullptr;
    f.vm->setBenchmarkName("Bench 2");
    ASSERT_TRUE(f.vm->save()["ok"].toBool());
    EXPECT_TRUE(sameToken(f.uc->saveCalls.back().second, admitted));
}

TEST(BenchmarkTableSave, EveryFailureRetainsRetryState) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    for (const auto error: {BenchmarkSaveError::EmptyName, BenchmarkSaveError::UnknownTarget,
                            BenchmarkSaveError::StaleToken, BenchmarkSaveError::Conflict,
                            BenchmarkSaveError::WriteFailed}) {
        TableFixture f;
        f.open(pasteBench());
        const auto originalToken = BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d1"};
        ASSERT_TRUE(f.vm->editThresholdText("a", "t1", "oops")["ok"].toBool());
        ASSERT_TRUE(f.vm->renameScenario("b", "Bee")["ok"].toBool());
        f.vm->setSelectedEntryIds({"b"});
        f.vm->setCurrentCell(QVariantMap{{"entryId", "a"}, {"tierId", "t1"}});
        const Benchmark before = working(f);
        f.uc->nextSaveOutcome = {std::nullopt, error};
        f.uc->duringSave = [&] { f.publish(); };

        EXPECT_FALSE(f.vm->save()["ok"].toBool());

        const int label = static_cast<int>(error);
        EXPECT_EQ(working(f), before) << label;
        EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "oops") << label;
        EXPECT_TRUE(f.vm->dirty()) << label;
        EXPECT_EQ(f.vm->normalizationIssues().size(), 1) << label;
        EXPECT_EQ(f.vm->selectedEntryIds(), QStringList{"b"}) << label;
        EXPECT_EQ(f.vm->currentCell()["entryId"].toString(), "a") << label;
        ASSERT_TRUE(canUndo(*f.vm)) << label;

        f.uc->duringSave = nullptr;
        f.uc->nextSaveOutcome = {BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d2"}, std::nullopt};
        ASSERT_TRUE(f.vm->save()["ok"].toBool()) << label;
        EXPECT_TRUE(sameToken(f.uc->saveCalls.back().second, originalToken)) << "retry keeps the original lease";

    }
}

TEST(BenchmarkTableUndo, LegacyThresholdCommandsKeepRetainedInputInHistory) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(pasteBench());
    const auto rawAt = [&] { return f.cell(f.rowOf("b"), f.columnOf("t1"), TM::DisplayTextRole).toString(); };

    ASSERT_TRUE(f.vm->editThresholdText("b", "t1", "oops")["ok"].toBool());
    ASSERT_TRUE(f.vm->setThreshold("b", "t1", 5)["ok"].toBool());
    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(rawAt(), "oops") << "undoing a typed replacement restores the retained text";

    ASSERT_TRUE(f.vm->clearThreshold("b", "t1")["ok"].toBool());
    EXPECT_EQ(f.cell(f.rowOf("b"), f.columnOf("t1"), TM::InputStateRole).toString(), QString());
    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(rawAt(), "oops") << "clearing retained text is its own Undo step";
}

TEST(BenchmarkTableUndo, TransitionsAnnounceClearedSelection) {
    TableFixture f;
    f.open(pasteBench());
    f.vm->setSelectedEntryIds({"a"});
    f.vm->setCurrentCell(QVariantMap{{"entryId", "a"}});
    QSignalSpy selection(f.vm.get(), &BenchmarkManagerViewModel::selectionChanged);

    f.vm->discard();

    EXPECT_GE(selection.count(), 1);
    EXPECT_TRUE(f.vm->selectedEntryIds().isEmpty());
    EXPECT_TRUE(f.vm->currentCell().isEmpty());

    f.uc->nextOpenSeed = librarySeed(pasteBench());
    ASSERT_TRUE(f.vm->openBenchmark("b1"));
    f.vm->setSelectedEntryIds({"a"});
    selection.clear();
    ASSERT_TRUE(f.vm->openBenchmark("b1"));
    EXPECT_GE(selection.count(), 1);
    EXPECT_TRUE(f.vm->selectedEntryIds().isEmpty());
}

TEST(BenchmarkTableUndo, LostFocusFallsBackToAdjacentRow) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(pasteBench());
    ASSERT_TRUE(f.vm->pasteText(at("append"), "N1\t1\nN2\t2")["ok"].toBool());
    const auto created = QString::fromStdString(working(f).uncategorized.back().id.value);
    f.vm->setCurrentCell(QVariantMap{{"entryId", created}, {"tierId", "t1"}});

    ASSERT_TRUE(f.vm->undo()["ok"].toBool());

    EXPECT_EQ(f.vm->currentCell()["entryId"].toString(), "u") << "the nearest surviving row keeps focus";
    EXPECT_EQ(f.vm->currentCell()["tierId"].toString(), "t1");
}

// ── Clipboard ───────────────────────────────────────────────────────────────────────────────────

TEST(BenchmarkTableClipboard, ClipboardTextIsReadOnRequest) {
    QGuiApplication::clipboard()->setText(QStringLiteral("Name\t1\t2\n"));
    TableFixture f;
    f.open(pasteBench());

    QString text;
    ASSERT_TRUE(QMetaObject::invokeMethod(f.vm.get(), "clipboardText", Q_RETURN_ARG(QString, text)));

    EXPECT_EQ(text, QStringLiteral("Name\t1\t2\n"));
    EXPECT_EQ(f.uc->resolveCalls.size(), 1U) << "reading the clipboard is not an edit";
}

// ── Selection repair ────────────────────────────────────────────────────────────────────────────

namespace {
    QVariantMap cellAnchor(const QString &entryId, const QString &tierId = {}, int columnKind = 3) {
        QVariantMap anchor{{"entryId", entryId}, {"columnKind", columnKind}};
        if (!tierId.isEmpty()) anchor.insert("tierId", tierId);
        return anchor;
    }

    QStringList sorted(QStringList ids) {
        ids.sort();
        return ids;
    }
}

TEST(BenchmarkTableSelection, StructuralEditsKeepSurvivingAnchors) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(projectionBench());
    f.vm->setSelectedEntryIds({"a", "b"});
    f.vm->setCurrentCell(cellAnchor("a", "t1"));
    QSignalSpy selection(f.vm.get(), &BenchmarkManagerViewModel::selectionChanged);

    ASSERT_TRUE(f.vm->reorderTier("t1", 0)["ok"].toBool());
    ASSERT_TRUE(f.vm->reorderScenario("a", 1)["ok"].toBool());
    ASSERT_TRUE(f.vm->assignScenarios({"a"}, "q")["ok"].toBool());
    ASSERT_TRUE(f.vm->pasteText(at("threshold", "u", "t1"), "1\t2\t3\n4\t5\t6")["ok"].toBool());

    EXPECT_EQ(f.rowIds().size(), 7) << "the paste expanded the table";
    EXPECT_EQ(sorted(f.vm->selectedEntryIds()), (QStringList{"a", "b"}));
    EXPECT_EQ(f.vm->currentCell(), cellAnchor("a", "t1"));
    EXPECT_EQ(selection.count(), 0) << "surviving anchors are not re-announced";
}

TEST(BenchmarkTableSelection, RemovedCurrentRowFallsBackWithoutJoiningSelection) {
    TableFixture f;
    f.open(projectionBench());
    f.vm->setSelectedEntryIds({"c", "a"});
    f.vm->setCurrentCell(cellAnchor("c", "t1"));
    QSignalSpy selection(f.vm.get(), &BenchmarkManagerViewModel::selectionChanged);
    const auto resolves = f.uc->resolveCalls.size();

    ASSERT_TRUE(f.vm->removeScenario("c")["ok"].toBool());

    // c sat at visible row 2; d now occupies it.
    EXPECT_EQ(f.vm->currentCell(), cellAnchor("d", "t1"));
    EXPECT_EQ(f.vm->selectedEntryIds(), QStringList{"a"}) << "the fallback row is focused, not selected";
    EXPECT_EQ(selection.count(), 1) << "both anchors change in one announcement";
    EXPECT_EQ(f.uc->resolveCalls.size(), resolves + 1) << "repairing the selection is not an extra edit";

    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(f.vm->currentCell(), cellAnchor("c", "t1"));
    EXPECT_EQ(sorted(f.vm->selectedEntryIds()), (QStringList{"a", "c"}));
    EXPECT_FALSE(canUndo(*f.vm)) << "selection repair recorded no history";
}

TEST(BenchmarkTableSelection, RemovedCurrentTierFallsBackToScenarioColumn) {
    TableFixture f;
    f.open(projectionBench());
    f.vm->setSelectedEntryIds({"b"});
    f.vm->setCurrentCell(cellAnchor("a", "t1"));

    ASSERT_TRUE(f.vm->removeTier("t1")["ok"].toBool());

    EXPECT_EQ(f.vm->currentCell(), cellAnchor("a", {}, 2)) << "never an obsolete threshold column";
    EXPECT_EQ(f.vm->selectedEntryIds(), QStringList{"b"});

    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(f.vm->currentCell(), cellAnchor("a", "t1"));
}

TEST(BenchmarkTableSelection, RemovingEveryRowClearsBothAnchors) {
    TableFixture f;
    f.open(draftWith("Bench", {tier("t1", "One")}, {benchmarkEntry("u", "U", std::nullopt, {{"t1", 1}})}));
    f.vm->setSelectedEntryIds({"u"});
    f.vm->setCurrentCell(cellAnchor("u", "t1"));

    ASSERT_TRUE(f.vm->removeScenario("u")["ok"].toBool());

    EXPECT_TRUE(f.vm->selectedEntryIds().isEmpty());
    EXPECT_TRUE(f.vm->currentCell().isEmpty());
}

TEST(BenchmarkTableSelection, SessionBoundariesClearAnchors) {
    TableFixture f;
    f.open(projectionBench());
    const auto select = [&] {
        f.vm->setSelectedEntryIds({"a", "b"});
        f.vm->setCurrentCell(cellAnchor("a", "t1"));
    };
    select();
    f.uc->nextNewSeed = newSeed(projectionBench());
    f.vm->beginNewBenchmark();
    EXPECT_TRUE(f.vm->selectedEntryIds().isEmpty());
    EXPECT_TRUE(f.vm->currentCell().isEmpty());

    select();
    f.uc->nextOpenSeed = librarySeed(projectionBench());
    ASSERT_TRUE(f.vm->openBenchmark("b1"));
    EXPECT_TRUE(f.vm->selectedEntryIds().isEmpty());
    EXPECT_TRUE(f.vm->currentCell().isEmpty());

    select();
    f.vm->discard();
    EXPECT_TRUE(f.vm->selectedEntryIds().isEmpty());
    EXPECT_TRUE(f.vm->currentCell().isEmpty());
}

// The dialog commits an open cell editor before an addition, so the retained text is the earlier
// history step: the first Undo removes only the addition.
TEST(BenchmarkTableUndo, CommittedInputThenAdditionUndoInOrder) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(pasteBench());
    const auto rawAt = [&] { return f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(); };
    ASSERT_TRUE(f.vm->editThresholdText("a", "t1", "oops")["ok"].toBool());
    const auto withInput = working(f);
    const auto added = f.vm->addTier("Silver");
    ASSERT_TRUE(added["ok"].toBool());
    ASSERT_EQ(working(f).tiers.size(), 3U);

    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(working(f), withInput);
    EXPECT_EQ(f.columnOf(added["createdId"].toString()), -1);
    EXPECT_EQ(rawAt(), "oops") << "the committed text survives undoing the addition";

    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(rawAt(), "10");
    EXPECT_FALSE(canUndo(*f.vm));
}

// ── Batch removal ───────────────────────────────────────────────────────────────────────────────

TEST(BenchmarkTableRemoval, SelectedRowsAreOneEditAndOneUndo) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.open(projectionBench());
    ASSERT_TRUE(f.vm->editThresholdText("c", "t1", "oops")["ok"].toBool());
    ASSERT_TRUE(f.vm->editThresholdText("b", "t2", "keep")["ok"].toBool());
    f.vm->setSelectedEntryIds({"u", "a", "c"});
    f.vm->setCurrentCell(cellAnchor("a", "t1"));
    const Benchmark before = working(f);
    const auto resolves = f.uc->resolveCalls.size();
    QSignalSpy changed(f.vm.get(), &BenchmarkManagerViewModel::draftChanged);

    ASSERT_TRUE(f.vm->removeScenarios({"u", "a", "c"})["ok"].toBool());

    EXPECT_EQ(changed.count(), 1) << "one publication";
    EXPECT_EQ(f.uc->resolveCalls.size(), resolves + 1) << "one resolution";
    EXPECT_EQ(f.rowIds(), (QStringList{"b", "d", "e"}));
    EXPECT_EQ(f.cell(f.rowOf("b"), f.columnOf("t2"), TM::DisplayTextRole).toString(), "keep");
    EXPECT_EQ(f.vm->normalizationIssues().size(), 1) << "the removed row's retained input goes with it";
    EXPECT_TRUE(f.vm->selectedEntryIds().isEmpty());
    EXPECT_EQ(f.vm->currentCell()["entryId"].toString(), "b") << "focus falls back to the row now at a's place";

    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(working(f), before) << "IDs, hierarchy positions, hashes and thresholds all return";
    EXPECT_EQ(f.rowIds(), (QStringList{"a", "b", "c", "d", "e", "u"}));
    EXPECT_EQ(f.cell(f.rowOf("c"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "oops");
    EXPECT_EQ(f.cell(f.rowOf("c"), f.columnOf("t1"), TM::InputStateRole).toString(), "invalid");
    EXPECT_EQ(sorted(f.vm->selectedEntryIds()), (QStringList{"a", "c", "u"}));
    EXPECT_EQ(f.vm->currentCell(), cellAnchor("a", "t1"));

    // The two earlier cell edits remain as their own steps.
    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_FALSE(canUndo(*f.vm));
}

TEST(BenchmarkTableRemoval, UnknownIdRejectsTheWholeBatch) {
    TableFixture f;
    f.open(projectionBench());
    ASSERT_TRUE(f.vm->editThresholdText("a", "t1", "oops")["ok"].toBool());
    f.vm->setSelectedEntryIds({"a"});
    f.vm->setCurrentCell(cellAnchor("a", "t1"));
    const Benchmark before = working(f);
    const auto resolves = f.uc->resolveCalls.size();
    QSignalSpy changed(f.vm.get(), &BenchmarkManagerViewModel::draftChanged);
    QSignalSpy selection(f.vm.get(), &BenchmarkManagerViewModel::selectionChanged);

    const auto result = f.vm->removeScenarios({"a", "unknown"});

    EXPECT_FALSE(result["ok"].toBool());
    EXPECT_EQ(result["error"].toString(), QObject::tr("That scenario no longer exists."));
    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(selection.count(), 0);
    EXPECT_EQ(f.uc->resolveCalls.size(), resolves);
    EXPECT_EQ(f.rowIds(), (QStringList{"a", "b", "c", "d", "e", "u"}));
    EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "oops");
    EXPECT_EQ(f.vm->selectedEntryIds(), QStringList{"a"});
    ASSERT_TRUE(f.vm->undo()["ok"].toBool()) << "only the earlier edit is in history";
    EXPECT_FALSE(canUndo(*f.vm));
    EXPECT_NE(working(f), before);
}

TEST(BenchmarkTableRemoval, DuplicatesAndEmptyBatches) {
    TableFixture f;
    f.open(projectionBench());
    const auto resolves = f.uc->resolveCalls.size();
    QSignalSpy changed(f.vm.get(), &BenchmarkManagerViewModel::draftChanged);

    EXPECT_TRUE(f.vm->removeScenarios({})["ok"].toBool());
    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(f.uc->resolveCalls.size(), resolves);
    EXPECT_FALSE(canUndo(*f.vm)) << "an empty batch is a no-op";

    ASSERT_TRUE(f.vm->removeScenarios({"a", "a", "b", "a"})["ok"].toBool());
    EXPECT_EQ(f.rowIds(), (QStringList{"c", "d", "e", "u"}));
    EXPECT_EQ(changed.count(), 1);
    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_FALSE(canUndo(*f.vm));
    EXPECT_EQ(f.rowIds(), (QStringList{"a", "b", "c", "d", "e", "u"}));

    TableFixture closed;
    closed.build();
    EXPECT_FALSE(closed.vm->removeScenarios({"a"})["ok"].toBool());
}

TEST(BenchmarkTableRemoval, EveryRowAcrossGroupsKeepsGroupsAndLease) {
    TableFixture f;
    f.open(projectionBench());
    const auto originalToken = BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d1"};
    f.vm->setSelectedEntryIds({"a", "e"});
    f.vm->setCurrentCell(cellAnchor("d", "t2"));

    // Direct category rows, two subcategories and Uncategorized in one command.
    ASSERT_TRUE(f.vm->removeScenarios({"e", "a", "u", "c", "b", "d"})["ok"].toBool());

    EXPECT_EQ(f.model->rowCount(), 0);
    EXPECT_EQ(working(f).categories.size(), 2U);
    EXPECT_EQ(working(f).categories.at(1).subcategories.size(), 2U);
    EXPECT_TRUE(working(f).uncategorized.empty());
    EXPECT_TRUE(f.vm->selectedEntryIds().isEmpty());
    EXPECT_TRUE(f.vm->currentCell().isEmpty());
    EXPECT_TRUE(f.vm->dirty());

    f.uc->nextSaveOutcome = {BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d2"}, std::nullopt};
    ASSERT_TRUE(f.vm->save()["ok"].toBool());
    EXPECT_TRUE(sameToken(f.uc->saveCalls.back().second, originalToken)) << "the batch never touched the lease";
}

// ── Application notifications ───────────────────────────────────────────────────────────────────

// A profile rebuild or library refresh changes what the draft's scenarios resolve to. The table
// must show that at once, not after the next edit, without touching the local session.
TEST(BenchmarkTableNotifications, RefreshesDraftResolutionWithoutEdit) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.uc->nextResolveResult = {unresolvedEntry("a")};
    f.open(projectionBench());
    ASSERT_TRUE(f.vm->editThresholdText("a", "t1", "oops")["ok"].toBool());
    f.vm->setSelectedEntryIds({"a", "b"});
    f.vm->setCurrentCell(cellAnchor("a", "t1"));
    const Benchmark draftBefore = working(f);
    const auto resolves = f.uc->resolveCalls.size();
    const auto mapping = [&](int role) { return f.cell(f.rowOf("a"), 2, role); };
    ASSERT_EQ(mapping(TM::MappingStateRole).toString(), "unresolved");

    f.uc->nextResolveResult = {resolvedEntry("a", "ha")};
    f.publish();

    EXPECT_EQ(f.uc->resolveCalls.size(), resolves + 1);
    EXPECT_EQ(mapping(TM::MappingStateRole).toString(), "resolved");
    EXPECT_EQ(working(f), draftBefore) << "the refresh resolves the unchanged local draft";
    EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::EditTextRole).toString(), "oops");
    EXPECT_EQ(sorted(f.vm->selectedEntryIds()), (QStringList{"a", "b"}));
    EXPECT_EQ(f.vm->currentCell(), cellAnchor("a", "t1"));
    EXPECT_TRUE(f.vm->dirty());

    // Refreshed candidate metadata reaches the row as well.
    f.uc->nextResolveResult = {ambiguousEntry("a", {"h1", "h2"})};
    f.publish();
    EXPECT_EQ(mapping(TM::MappingStateRole).toString(), "ambiguous");
    EXPECT_EQ(mapping(TM::MappingCandidatesRole).toList().size(), 2);
    f.uc->nextResolveResult = {ambiguousEntry("a", {"h1", "h2", "h3"})};
    f.publish();
    EXPECT_EQ(mapping(TM::MappingCandidatesRole).toList().size(), 3);

    // Undo still reverses the last local edit, now against the latest profile.
    ASSERT_TRUE(canUndo(*f.vm)) << "publications add or drop no history";
    ASSERT_TRUE(f.vm->undo()["ok"].toBool());
    EXPECT_EQ(scoreOf(working(f), "a", "t1"), 2.0);
    EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::InputStateRole).toString(), QString());
    EXPECT_EQ(mapping(TM::MappingCandidatesRole).toList().size(), 3);
    EXPECT_FALSE(canUndo(*f.vm));

    TableFixture closed;
    closed.build();
    closed.publish();
    EXPECT_TRUE(closed.uc->resolveCalls.empty()) << "no draft, nothing to resolve";
}

TEST(BenchmarkTableNotifications, PublicationsInsideSaveKeepRetryState) {
    const DefaultLocaleGuard guard{QLocale{QLocale::English, QLocale::UnitedKingdom}};
    TableFixture f;
    f.uc->nextResolveResult = {unresolvedEntry("a")};
    f.open(projectionBench());
    const auto originalToken = BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d1"};
    ASSERT_TRUE(f.vm->editThresholdText("a", "t1", "oops")["ok"].toBool());
    const Benchmark draftBefore = working(f);
    QString mappingDuringSave;
    QString rawDuringSave;
    f.uc->duringSave = [&] {
        f.uc->nextResolveResult = {resolvedEntry("a", "ha")};
        f.publish();
        mappingDuringSave = f.cell(f.rowOf("a"), 2, TM::MappingStateRole).toString();
        rawDuringSave = f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString();
    };

    f.uc->nextSaveOutcome = {std::nullopt, BenchmarkSaveError::WriteFailed};
    EXPECT_FALSE(f.vm->save()["ok"].toBool());
    EXPECT_EQ(mappingDuringSave, "resolved");
    EXPECT_EQ(rawDuringSave, "oops");
    EXPECT_EQ(working(f), draftBefore) << "the refresh resolved the live draft, not the save candidate";
    EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::DisplayTextRole).toString(), "oops");
    EXPECT_TRUE(canUndo(*f.vm));

    f.uc->nextSaveOutcome = {BenchmarkEditToken{BenchmarkId{"b1"}, "b1.json", "d2"}, std::nullopt};
    ASSERT_TRUE(f.vm->save()["ok"].toBool());
    EXPECT_EQ(rawDuringSave, "oops") << "input survives until the save is accepted";
    EXPECT_TRUE(sameToken(f.uc->saveCalls.back().second, originalToken));
    EXPECT_EQ(working(f), f.uc->saveCalls.back().first) << "success adopts the normalized candidate";
    EXPECT_EQ(f.cell(f.rowOf("a"), f.columnOf("t1"), TM::InputStateRole).toString(), QString());
    EXPECT_EQ(f.cell(f.rowOf("a"), 2, TM::MappingStateRole).toString(), "resolved");
    EXPECT_FALSE(canUndo(*f.vm));
}
