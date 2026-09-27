#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_editor.h"
#include "benchmarks/benchmark_validation.h"

using namespace ksv::domain;

namespace {
    std::function<std::string()> countingIds() {
        auto n = std::make_shared<int>(0);
        return [n] { return "id-" + std::to_string(++*n); };
    }

    bool hasIssue(const CompletenessResult &result, BenchmarkIssueCode code) {
        for (const auto &issue: result.issues)
            if (issue.code == code) return true;
        return false;
    }
}

TEST(BenchmarkEditor, AddTierAppendsWithFreshId) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};

    const auto result = editor.addTier("Gold");

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.createdTier.has_value());
    ASSERT_EQ(bench.tiers.size(), 1U);
    EXPECT_EQ(bench.tiers.front().name, "Gold");
    EXPECT_EQ(bench.tiers.front().id, *result.createdTier);
}

TEST(BenchmarkEditor, ReorderTierMovesTheTier) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    editor.addTier("Bronze");
    editor.addTier("Silver");
    editor.addTier("Gold");
    const auto goldId = bench.tiers.at(2).id;

    const auto result = editor.reorderTier(goldId, 0);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(bench.tiers.size(), 3U);
    EXPECT_EQ(bench.tiers.at(0).name, "Gold");
    EXPECT_EQ(bench.tiers.at(2).name, "Silver");
}

TEST(BenchmarkEditor, ReorderTierOutOfRangeReturnsPositionOutOfRange) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto tier = *editor.addTier("Gold").createdTier;

    EXPECT_EQ(editor.reorderTier(tier, 1).error,
              std::make_optional(BenchmarkEditError::PositionOutOfRange));
    EXPECT_EQ(editor.reorderTier(TierId{"nope"}, 0).error,
              std::make_optional(BenchmarkEditError::UnknownTier));
}

TEST(BenchmarkEditor, TierCommandOnUnknownTierReturnsUnknownTier) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};

    EXPECT_EQ(editor.renameTier(TierId{"nope"}, "x").error,
              std::make_optional(BenchmarkEditError::UnknownTier));
    EXPECT_EQ(editor.removeTier(TierId{"nope"}).error,
              std::make_optional(BenchmarkEditError::UnknownTier));
}

TEST(BenchmarkEditor, RemoveTierAlsoRemovesItsThresholdsFromEveryEntry) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto tier = *editor.addTier("Gold").createdTier;
    const auto otherTier = *editor.addTier("Silver").createdTier;
    const auto entry = *editor.addUnplayedScenario("1w6ts").createdEntry;
    ASSERT_TRUE(editor.setThreshold(entry, tier, 10.0).ok());
    ASSERT_TRUE(editor.setThreshold(entry, otherTier, 5.0).ok());

    const auto result = editor.removeTier(tier);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(bench.uncategorized.size(), 1U);
    // Only the removed tier's threshold is dropped; the surviving tier's stays.
    const auto &remaining = bench.uncategorized.front().thresholds;
    ASSERT_EQ(remaining.size(), 1U);
    EXPECT_EQ(remaining.front().tierId, otherTier);
    ASSERT_EQ(bench.tiers.size(), 1U);
    EXPECT_EQ(bench.tiers.front().id, otherTier);
}

TEST(BenchmarkEditor, AddScenarioRejectsDuplicateDisplayName) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    ASSERT_TRUE(editor.addUnplayedScenario("1w6ts").ok());

    EXPECT_EQ(editor.addUnplayedScenario("1w6ts").error,
              std::make_optional(BenchmarkEditError::DuplicateScenarioName));
    EXPECT_EQ(editor.addKnownScenario("1w6ts", "hash").error,
              std::make_optional(BenchmarkEditError::DuplicateScenarioName));
    EXPECT_EQ(bench.uncategorized.size(), 1U);
}

TEST(BenchmarkEditor, AddKnownScenarioPersistsHash) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};

    const auto result = editor.addKnownScenario("1w6ts", "C0FFE00D");

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(bench.uncategorized.size(), 1U);
    ASSERT_TRUE(bench.uncategorized.front().hash.has_value());
    EXPECT_EQ(*bench.uncategorized.front().hash, "C0FFE00D");
}

TEST(BenchmarkEditor, SetThresholdUpsertsForTier) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto tier = *editor.addTier("Gold").createdTier;
    const auto entry = *editor.addUnplayedScenario("1w6ts").createdEntry;

    ASSERT_TRUE(editor.setThreshold(entry, tier, 10.0).ok());
    ASSERT_TRUE(editor.setThreshold(entry, tier, 20.0).ok());

    ASSERT_EQ(bench.uncategorized.front().thresholds.size(), 1U);
    EXPECT_EQ(bench.uncategorized.front().thresholds.front().score, 20.0);
    EXPECT_EQ(bench.uncategorized.front().thresholds.front().tierId, tier);
}

TEST(BenchmarkEditor, ClearThresholdRemovesOnlyThatTiersThreshold) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto gold = *editor.addTier("Gold").createdTier;
    const auto silver = *editor.addTier("Silver").createdTier;
    const auto entry = *editor.addUnplayedScenario("1w6ts").createdEntry;
    ASSERT_TRUE(editor.setThreshold(entry, gold, 10.0).ok());
    ASSERT_TRUE(editor.setThreshold(entry, silver, 5.0).ok());

    ASSERT_TRUE(editor.clearThreshold(entry, gold).ok());

    ASSERT_EQ(bench.uncategorized.front().thresholds.size(), 1U);
    EXPECT_EQ(bench.uncategorized.front().thresholds.front().tierId, silver);
}

TEST(BenchmarkEditor, SetThresholdOnUnknownEntryOrTierReturnsError) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto tier = *editor.addTier("Gold").createdTier;
    const auto entry = *editor.addUnplayedScenario("1w6ts").createdEntry;

    EXPECT_EQ(editor.setThreshold(entry, TierId{"nope"}, 1.0).error,
              std::make_optional(BenchmarkEditError::UnknownTier));
    EXPECT_EQ(editor.setThreshold(ScenarioEntryId{"nope"}, tier, 1.0).error,
              std::make_optional(BenchmarkEditError::UnknownEntry));
    EXPECT_EQ(editor.clearThreshold(ScenarioEntryId{"nope"}, tier).error,
              std::make_optional(BenchmarkEditError::UnknownEntry));
}

TEST(BenchmarkEditor, RenameScenarioUpdatesTheRetainedDisplayName) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto entry = *editor.addUnplayedScenario("old name").createdEntry;

    ASSERT_TRUE(editor.renameScenario(entry, "new name").ok());

    EXPECT_EQ(bench.uncategorized.front().name, "new name");
}

TEST(BenchmarkEditor, AddSubcategoryToPopulatedCategoryMovesDirectScenariosOnlyWhenChosen) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    editor.renameBenchmark("Bench");
    const auto category = *editor.addCategory("Clicking").createdGroup;
    const auto first = *editor.addUnplayedScenario("s1").createdEntry;
    const auto second = *editor.addUnplayedScenario("s2").createdEntry;
    ASSERT_TRUE(editor.moveScenario(first, category).ok());
    ASSERT_TRUE(editor.moveScenario(second, category).ok());

    EXPECT_EQ(editor.addSubcategory(category, "Static").error, BenchmarkEditError::MixedContent);
    const auto result = editor.addSubcategory(category, "Static", RelocateDirectToNewSubcategory{});

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.createdGroup.has_value());
    ASSERT_EQ(bench.categories.size(), 1U);
    EXPECT_TRUE(bench.categories.front().scenarios.empty());
    ASSERT_EQ(bench.categories.front().subcategories.size(), 1U);
    EXPECT_EQ(bench.categories.front().subcategories.front().name, "Static");
    ASSERT_EQ(bench.categories.front().subcategories.front().scenarios.size(), 2U);
    EXPECT_EQ(bench.categories.front().subcategories.front().scenarios.front().name, "s1");
    EXPECT_FALSE(hasIssue(editor.validation(), BenchmarkIssueCode::MixedCategoryContent));
}

TEST(BenchmarkEditor, RemoveCategoryReturnsScenariosToUncategorized) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto category = *editor.addCategory("Clicking").createdGroup;
    const auto entry = *editor.addUnplayedScenario("s1").createdEntry;
    ASSERT_TRUE(editor.moveScenario(entry, category).ok());

    const auto result = editor.removeCategory(category);

    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(bench.categories.empty());
    ASSERT_EQ(bench.uncategorized.size(), 1U);
    EXPECT_EQ(bench.uncategorized.front().name, "s1");
}

TEST(BenchmarkEditor, RemoveSubcategoryReturnsItsScenariosToUncategorized) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto category = *editor.addCategory("Clicking").createdGroup;
    const auto subcategory = *editor.addSubcategory(category, "Static").createdGroup;
    const auto entry = *editor.addUnplayedScenario("s1").createdEntry;
    ASSERT_TRUE(editor.moveScenario(entry, subcategory).ok());
    ASSERT_EQ(bench.categories.front().subcategories.size(), 1U);

    const auto result = editor.removeCategory(subcategory);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(bench.categories.size(), 1U);
    EXPECT_TRUE(bench.categories.front().subcategories.empty());
    ASSERT_EQ(bench.uncategorized.size(), 1U);
    EXPECT_EQ(bench.uncategorized.front().name, "s1");
}

TEST(BenchmarkEditor, MoveScenarioIntoCategoryThatHasSubcategoriesReturnsMixedContent) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto category = *editor.addCategory("Clicking").createdGroup;
    ASSERT_TRUE(editor.addUnplayedScenario("s1").ok());
    ASSERT_TRUE(editor.addSubcategory(category, "Static").ok());
    const auto entry = *editor.addUnplayedScenario("s2").createdEntry;

    EXPECT_EQ(editor.moveScenario(entry, category).error,
              std::make_optional(BenchmarkEditError::MixedContent));

    // The rejected move left the entry where it was.
    ASSERT_EQ(bench.uncategorized.size(), 2U);
    EXPECT_EQ(bench.uncategorized.back().name, "s2");
}

TEST(BenchmarkEditor, MoveScenarioBackToUncategorized) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto category = *editor.addCategory("Clicking").createdGroup;
    const auto entry = *editor.addUnplayedScenario("s1").createdEntry;
    ASSERT_TRUE(editor.moveScenario(entry, category).ok());
    ASSERT_EQ(bench.categories.front().scenarios.size(), 1U);

    const auto result = editor.moveScenario(entry, std::monostate{});

    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(bench.categories.front().scenarios.empty());
    ASSERT_EQ(bench.uncategorized.size(), 1U);
    EXPECT_EQ(bench.uncategorized.front().id, entry);
}

TEST(BenchmarkEditor, MoveScenarioToUnknownGroupReturnsUnknownGroup) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    const auto entry = *editor.addUnplayedScenario("s1").createdEntry;

    EXPECT_EQ(editor.moveScenario(entry, GroupId{"nope"}).error,
              std::make_optional(BenchmarkEditError::UnknownGroup));
}

namespace {
    // An ID factory that fails the test if a rejected command allocates.
    std::function<std::string()> forbiddenIds() {
        return [] {
            ADD_FAILURE() << "a rejected command allocated an ID";
            return std::string("forbidden");
        };
    }

    ScenarioEntry scored(const std::string &id, const std::string &name, std::optional<std::string> hash,
                         double score) {
        return ScenarioEntry{ScenarioEntryId{id}, name, std::move(hash), {Threshold{TierId{"t1"}, score}}};
    }

    std::vector<std::string> idsOf(const std::vector<ScenarioEntry> &entries) {
        std::vector<std::string> ids;
        for (const auto &entry: entries) ids.push_back(entry.id.value);
        return ids;
    }

    // Visible rows a,b | c,d across two categories; g3 already holds x; g4 has subcategories.
    Benchmark groupedBench() {
        Benchmark bench;
        bench.name = "Bench";
        bench.tiers = {Tier{TierId{"t1"}, "T1", {}}};
        bench.categories = {
            Category{GroupId{"g1"}, "One", {}, {scored("a", "A", "ha", 1), scored("b", "B", "hb", 2)}, {}},
            Category{GroupId{"g2"}, "Two", {}, {scored("c", "C", "hc", 3), scored("d", "D", std::nullopt, 4)}, {}},
            Category{GroupId{"g3"}, "Three", {}, {scored("x", "X", "hx", 5)}, {}},
            Category{GroupId{"g4"}, "Four", {}, {},
                     {Subcategory{GroupId{"s4"}, "Sub", {}, {scored("y", "Y", std::nullopt, 6)}}}},
        };
        return bench;
    }
}

TEST(BenchmarkTableRows, RepeatedUnnamedAppend) {
    Benchmark bench;
    bench.uncategorized = {ScenarioEntry{ScenarioEntryId{"u0"}, "", std::nullopt, {}},
                           scored("n1", "Named", "h1", 10)};
    const auto before = bench.uncategorized;
    BenchmarkEditor editor{bench, countingIds()};

    const auto result = editor.appendUnnamedScenarios(2);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.createdEntries.size(), 2U);
    EXPECT_NE(result.createdEntries.at(0), result.createdEntries.at(1));
    ASSERT_EQ(bench.uncategorized.size(), 4U);
    EXPECT_EQ(bench.uncategorized.at(0), before.at(0));
    EXPECT_EQ(bench.uncategorized.at(1), before.at(1));
    for (std::size_t i = 0; i < 2; ++i) {
        const auto &created = bench.uncategorized.at(2 + i);
        EXPECT_EQ(created.id, result.createdEntries.at(i));
        EXPECT_TRUE(created.name.empty());
        EXPECT_FALSE(created.hash.has_value());
        EXPECT_TRUE(created.thresholds.empty());
    }
}

TEST(BenchmarkTableAssignment, OrderedAssignment) {
    Benchmark bench = groupedBench();
    const Benchmark before = bench;
    BenchmarkEditor editor{bench, forbiddenIds()};

    const auto result = editor.assignScenarios({ScenarioEntryId{"a"}, ScenarioEntryId{"c"}}, GroupId{"g3"});

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(idsOf(bench.categories.at(2).scenarios), (std::vector<std::string>{"x", "a", "c"}));
    EXPECT_EQ(idsOf(bench.categories.at(0).scenarios), std::vector<std::string>{"b"});
    EXPECT_EQ(idsOf(bench.categories.at(1).scenarios), std::vector<std::string>{"d"});
    EXPECT_EQ(bench.categories.at(2).scenarios.at(1), before.categories.at(0).scenarios.at(0));
    EXPECT_EQ(bench.categories.at(2).scenarios.at(2), before.categories.at(1).scenarios.at(0));
}

TEST(BenchmarkTableAssignment, RejectedBatchIsUnchanged) {
    const auto attempt = [](const std::vector<ScenarioEntryId> &ids, const EditorGroupTarget &to) {
        Benchmark bench = groupedBench();
        const Benchmark before = bench;
        BenchmarkEditor editor{bench, forbiddenIds()};
        const auto result = editor.assignScenarios(ids, to);
        EXPECT_EQ(bench, before);
        return result.error;
    };

    EXPECT_EQ(attempt({ScenarioEntryId{"a"}, ScenarioEntryId{"missing"}}, GroupId{"g3"}),
              BenchmarkEditError::UnknownEntry);
    EXPECT_EQ(attempt({ScenarioEntryId{"a"}}, GroupId{"nowhere"}), BenchmarkEditError::UnknownGroup);
    EXPECT_EQ(attempt({ScenarioEntryId{"a"}, ScenarioEntryId{"c"}}, GroupId{"g4"}),
              BenchmarkEditError::MixedContent);
}

TEST(BenchmarkTableHierarchy, NoChoiceDoesNotReorganize) {
    Benchmark bench = groupedBench();
    const Benchmark before = bench;
    BenchmarkEditor editor{bench, forbiddenIds()};

    const auto result = editor.addSubcategory(GroupId{"g1"}, "Static");

    EXPECT_EQ(result.error, BenchmarkEditError::MixedContent);
    EXPECT_EQ(bench, before);
}

TEST(BenchmarkTableHierarchy, ChosenRelocation) {
    {
        Benchmark bench = groupedBench();
        const auto direct = bench.categories.at(0).scenarios;
        BenchmarkEditor editor{bench, countingIds()};

        const auto result = editor.addSubcategory(GroupId{"g1"}, "Static", RelocateDirectToNewSubcategory{});

        ASSERT_TRUE(result.ok());
        const auto &category = bench.categories.at(0);
        EXPECT_TRUE(category.scenarios.empty());
        ASSERT_EQ(category.subcategories.size(), 1U);
        EXPECT_EQ(category.subcategories.front().id, result.createdGroup);
        EXPECT_EQ(category.subcategories.front().name, "Static");
        EXPECT_EQ(category.subcategories.front().scenarios, direct);
        EXPECT_FALSE(hasIssue(editor.validation(), BenchmarkIssueCode::MixedCategoryContent));
    }
    {
        Benchmark bench = groupedBench();
        bench.uncategorized = {scored("u", "U", std::nullopt, 7)};
        const auto direct = bench.categories.at(0).scenarios;
        BenchmarkEditor editor{bench, countingIds()};

        const auto result = editor.addSubcategory(GroupId{"g1"}, "Static", RelocateDirectToUncategorized{});

        ASSERT_TRUE(result.ok());
        const auto &category = bench.categories.at(0);
        EXPECT_TRUE(category.scenarios.empty());
        ASSERT_EQ(category.subcategories.size(), 1U);
        EXPECT_EQ(category.subcategories.front().id, result.createdGroup);
        EXPECT_TRUE(category.subcategories.front().scenarios.empty());
        EXPECT_EQ(idsOf(bench.uncategorized), (std::vector<std::string>{"u", "a", "b"}));
        EXPECT_EQ(bench.uncategorized.at(1), direct.at(0));
        EXPECT_EQ(bench.uncategorized.at(2), direct.at(1));
    }
}

TEST(BenchmarkTableOrdering, ReorderWithinCollections) {
    Benchmark bench;
    bench.tiers = {Tier{TierId{"t1"}, "T1", {}}};
    bench.categories = {Category{GroupId{"g"}, "G", {}, {}, {
        Subcategory{GroupId{"p"}, "P", {}, {scored("a", "A", "ha", 1), scored("b", "B", "hb", 2)}},
        Subcategory{GroupId{"q"}, "Q", {}, {scored("c", "C", "hc", 3)}},
    }}};
    const Benchmark before = bench;
    BenchmarkEditor editor{bench, forbiddenIds()};

    ASSERT_TRUE(editor.reorderSubcategory(GroupId{"q"}, 0).ok());
    ASSERT_TRUE(editor.reorderScenario(ScenarioEntryId{"b"}, 0).ok());

    const auto &subs = bench.categories.front().subcategories;
    ASSERT_EQ(subs.size(), 2U);
    EXPECT_EQ(subs.at(0), before.categories.front().subcategories.at(1));
    EXPECT_EQ(subs.at(1).id, GroupId{"p"});
    EXPECT_EQ(idsOf(subs.at(1).scenarios), (std::vector<std::string>{"b", "a"}));
    EXPECT_EQ(subs.at(1).scenarios.at(0), before.categories.front().subcategories.at(0).scenarios.at(1));

    const Benchmark reordered = bench;
    EXPECT_EQ(editor.reorderSubcategory(GroupId{"unknown"}, 0).error, BenchmarkEditError::UnknownGroup);
    EXPECT_EQ(editor.reorderSubcategory(GroupId{"p"}, 2).error, BenchmarkEditError::PositionOutOfRange);
    EXPECT_EQ(editor.reorderScenario(ScenarioEntryId{"unknown"}, 0).error, BenchmarkEditError::UnknownEntry);
    EXPECT_EQ(editor.reorderScenario(ScenarioEntryId{"a"}, 2).error, BenchmarkEditError::PositionOutOfRange);
    EXPECT_EQ(bench, reordered);
}
