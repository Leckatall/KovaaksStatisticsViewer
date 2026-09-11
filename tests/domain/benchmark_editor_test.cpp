#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <variant>

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

TEST(BenchmarkEditor, AddSubcategoryToPopulatedCategoryMovesDirectScenariosIntoASubcategory) {
    Benchmark bench;
    BenchmarkEditor editor{bench, countingIds()};
    editor.renameBenchmark("Bench");
    const auto category = *editor.addCategory("Clicking").createdGroup;
    const auto first = *editor.addUnplayedScenario("s1").createdEntry;
    const auto second = *editor.addUnplayedScenario("s2").createdEntry;
    ASSERT_TRUE(editor.moveScenario(first, category).ok());
    ASSERT_TRUE(editor.moveScenario(second, category).ok());

    const auto result = editor.addSubcategory(category, "Static");

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.createdGroup.has_value());
    ASSERT_EQ(bench.categories.size(), 1U);
    EXPECT_TRUE(bench.categories.front().scenarios.empty());
    ASSERT_EQ(bench.categories.front().subcategories.size(), 2U);
    ASSERT_EQ(bench.categories.front().subcategories.front().scenarios.size(), 2U);
    EXPECT_EQ(bench.categories.front().subcategories.front().scenarios.front().name, "s1");
    EXPECT_EQ(bench.categories.front().subcategories.back().name, "Static");
    EXPECT_TRUE(bench.categories.front().subcategories.back().scenarios.empty());
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
