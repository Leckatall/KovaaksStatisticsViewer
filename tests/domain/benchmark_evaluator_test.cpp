#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>

#include "benchmark_builders.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_evaluator.h"
#include "run_builders.h"
#include "user_profile.h"

using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    Tier tier(const std::string &id) { return {TierId{id}, id, {}}; }
    std::vector<Tier> ladder3() { return {tier("bronze"), tier("silver"), tier("gold")}; }
    std::vector<Threshold> steps(std::initializer_list<std::pair<std::string, double> > values) {
        std::vector<Threshold> result;
        for (const auto &[id, score]: values) result.push_back({TierId{id}, score});
        return result;
    }
    BenchmarkDefinition benchmark() {
        BenchmarkDefinition value{BenchmarkId{"bench"}, "Bench", ladder3(), {}, {}};
        value.uncategorized.push_back(benchmarkEntry("entry", "Alpha", "hash",
            {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}}));
        return value;
    }
    constexpr long long kDay = 86'400'000LL;

    BenchmarkDefinition hierarchyBenchmark() {
        const std::initializer_list<std::pair<std::string, double> > rungs{
            {"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}};
        BenchmarkDefinition value{BenchmarkId{"hierarchy"}, "Hierarchy", ladder3(), {}, {}};
        value.uncategorized = {benchmarkEntry("u1", "U1", "u1", rungs),
                               benchmarkEntry("u2", "U2", "u2", rungs)};
        Category direct{GroupId{"direct"}, "Direct", {}, {}, {}};
        direct.scenarios = {benchmarkEntry("d1", "D1", "d1", rungs),
                            benchmarkEntry("d2", "D2", "d2", rungs)};
        Category nested{GroupId{"nested"}, "Nested", {}, {}, {}};
        Subcategory first{GroupId{"first"}, "First", {}, {}};
        first.scenarios = {benchmarkEntry("a1", "A1", "a1", rungs),
                           benchmarkEntry("a2", "A2", "a2", rungs)};
        Subcategory second{GroupId{"second"}, "Second", {}, {}};
        second.scenarios = {benchmarkEntry("b1", "B1", "b1", rungs)};
        nested.subcategories = {first, second};
        value.categories = {direct, nested};
        return value;
    }

    std::vector<ScenarioResolution> hierarchyResolutions() {
        return {resolvedEntry("u1"), resolvedEntry("u2"),
                resolvedEntry("d1"), resolvedEntry("d2"),
                resolvedEntry("a1"), resolvedEntry("a2"),
                resolvedEntry("b1")};
    }

    std::vector<RunFact> hierarchyScores(
        const std::initializer_list<std::pair<std::string, float> > lowered = {}) {
        std::vector<std::pair<std::string, float> > scores{
            {"u1", 300.0F}, {"u2", 300.0F}, {"d1", 300.0F}, {"d2", 300.0F},
            {"a1", 300.0F}, {"a2", 300.0F}, {"b1", 300.0F}};
        for (auto &[hash, score]: scores)
            for (const auto &[loweredHash, loweredScore]: lowered)
                if (hash == loweredHash) score = loweredScore;
        std::vector<RunFact> facts;
        for (const auto &[hash, score]: scores) facts.push_back(benchmarkFact(hash, 100 * kDay, score));
        return facts;
    }

    const GroupEvaluation &groupNamed(const Benchmark &evaluated, const std::string &id) {
        for (const auto *category: evaluated.categories()) {
            if (category->id().value == id) return *category;
            for (const auto *subcategory: category->subgroups())
                if (subcategory->id().value == id) return *subcategory;
        }
        throw std::out_of_range("unknown group");
    }

    BenchmarkDefinition twoScenarioBenchmark() {
        BenchmarkDefinition value{BenchmarkId{"two"}, "Two", ladder3(), {}, {}};
        value.uncategorized = {
            benchmarkEntry("one", "One", "one", {{"bronze", 100}, {"silver", 200}, {"gold", 300}}),
            benchmarkEntry("two", "Two", "two", {{"bronze", 100}, {"silver", 200}, {"gold", 300}})};
        return value;
    }
}

TEST(BenchmarkThresholdLadder, OrdersByTierNotByThresholdPosition) {
    const std::vector<Tier> tiers{{TierId{"bronze"}, "Bronze", {}}, {TierId{"silver"}, "Silver", {}}};
    const std::vector<Threshold> thresholds{{TierId{"silver"}, 200.0}, {TierId{"bronze"}, 100.0}};
    EXPECT_EQ(*thresholdLadder(tiers, thresholds), (std::vector{100.0, 200.0}));
}

TEST(BenchmarkThresholdLadder, RejectsMissingDuplicateInvalidAndEmptyLadders) {
    EXPECT_FALSE(thresholdLadder(ladder3(), steps({{"bronze", 100}, {"gold", 300}})));
    EXPECT_FALSE(thresholdLadder(ladder3(), steps({{"bronze", 100}, {"bronze", 150}, {"silver", 200}, {"gold", 300}})));
    EXPECT_FALSE(thresholdLadder(ladder3(), steps({{"bronze", -1}, {"silver", 200}, {"gold", 300}})));
    EXPECT_FALSE(thresholdLadder(ladder3(), steps({{"bronze", std::numeric_limits<double>::infinity()},
                                                   {"silver", 200}, {"gold", 300}})));
    EXPECT_FALSE(thresholdLadder(ladder3(), steps({{"bronze", 100}, {"silver", 100}, {"gold", 300}})));
    EXPECT_FALSE(thresholdLadder({}, steps({{"bronze", 100}})));
}

TEST(BenchmarkNormalization, HandlesInterpolationBoundsAndUnreadableInput) {
    const auto thresholds = steps({{"bronze", 100}, {"silver", 200}, {"gold", 300}});
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, std::nullopt), 0.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, -10.0), 0.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, 25.0), .25);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, 100.0), 1.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, 150.0), 1.5);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, 999.0), 3.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, std::numeric_limits<double>::quiet_NaN()), 0.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, std::numeric_limits<double>::infinity()), 0.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), steps({{"bronze", 100}}), 500.0), 0.0);

    // A one-tier ladder has no interval above its only rung, so it tops out at 1.
    const std::vector<Tier> single{tier("bronze")};
    const auto onlyRung = steps({{"bronze", 100}});
    EXPECT_DOUBLE_EQ(normalizedTierValue(single, onlyRung, 50.0), 0.5);
    EXPECT_DOUBLE_EQ(normalizedTierValue(single, onlyRung, 100.0), 1.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(single, onlyRung, 1000.0), 1.0);

    auto definition = benchmark();
    definition.uncategorized.front().thresholds = thresholds;
    const auto evaluated = Benchmark(definition, {Completeness::Trackable, {}}, {resolvedEntry("entry")}, {});
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_DOUBLE_EQ(scenario.normalized(std::nullopt), 0.0);
    EXPECT_DOUBLE_EQ(scenario.normalized(-10.0), 0.0);
    EXPECT_DOUBLE_EQ(scenario.normalized(25.0), .25);
    EXPECT_DOUBLE_EQ(scenario.normalized(100.0), 1.0);
    EXPECT_DOUBLE_EQ(scenario.normalized(150.0), 1.5);
    EXPECT_DOUBLE_EQ(scenario.normalized(999.0), 3.0);
    EXPECT_DOUBLE_EQ(scenario.normalized(std::numeric_limits<double>::quiet_NaN()), 0.0);
    EXPECT_DOUBLE_EQ(scenario.normalized(std::numeric_limits<double>::infinity()), 0.0);

    definition.uncategorized.front().thresholds = steps({{"bronze", 100}});
    const auto partial = Benchmark(definition, {Completeness::Incomplete, {}}, {resolvedEntry("entry")}, {});
    EXPECT_DOUBLE_EQ(scenarioNamed(partial, "entry").normalized(500.0), 0.0);

    definition.tiers = single;
    const auto singleTier = Benchmark(definition, {Completeness::Trackable, {}}, {resolvedEntry("entry")}, {});
    const auto &singleTierScenario = scenarioNamed(singleTier, "entry");
    EXPECT_DOUBLE_EQ(singleTierScenario.normalized(50.0), 0.5);
    EXPECT_DOUBLE_EQ(singleTierScenario.normalized(100.0), 1.0);
    EXPECT_DOUBLE_EQ(singleTierScenario.normalized(1000.0), 1.0);
}

TEST(BenchmarkNormalization, ZeroFirstThresholdDistinguishesPlayedFromUnplayed) {
    const auto thresholds = steps({{"bronze", 0}, {"silver", 200}, {"gold", 300}});
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, 100.0), 1.5);

    auto definition = benchmark();
    definition.uncategorized.front().thresholds = thresholds;
    const auto evaluated = Benchmark(definition, {Completeness::Trackable, {}}, {resolvedEntry("entry")}, {});
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_DOUBLE_EQ(scenario.normalized(0.0), 1.0);
    EXPECT_DOUBLE_EQ(scenario.normalized(100.0), 1.5);
}

TEST(BenchmarkEvaluator, ProjectsPersonalBestRecentFiveAndTierProgress) {
    std::vector<RunFact> facts;
    for (int score = 10; score <= 60; score += 10)
        facts.push_back(benchmarkFact("hash", kDay * score, static_cast<float>(score), 3.0F));
    const auto evaluated = Benchmark(benchmark(), {Completeness::Trackable, {}},
                                     {resolvedEntry("entry")}, facts);
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_DOUBLE_EQ(*scenario.stats().personalBest(), 60.0);
    EXPECT_DOUBLE_EQ(*scenario.stats().recentAverage(), 40.0);
    EXPECT_EQ(scenario.stats().recentSampleCount(), 5);
    EXPECT_EQ(scenario.stats().runCount(), 6);
    EXPECT_FALSE(scenario.attainedTier());
    ASSERT_TRUE(scenario.nextThreshold());
    EXPECT_EQ(scenario.nextThreshold()->tierId.value, "bronze");
    EXPECT_DOUBLE_EQ(scenario.stats().playtimeSeconds(), 18.0);
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(), 18.0);
}

TEST(BenchmarkEvaluator, PersonalBestTimeIsTheEarliestRunThatScoredIt) {
    // Out of order on purpose: the later tie (day 9) is listed first.
    const auto facts = std::vector{benchmarkFact("hash", 9 * kDay, 80.0F), benchmarkFact("hash", 2 * kDay, 50.0F),
                                   benchmarkFact("hash", 5 * kDay, 80.0F), benchmarkFact("hash", 7 * kDay, 70.0F)};
    const auto evaluated = Benchmark(benchmark(), {Completeness::Trackable, {}},
                                     {resolvedEntry("entry")}, facts);
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_DOUBLE_EQ(*scenario.stats().personalBest(), 80.0);
    ASSERT_TRUE(scenario.stats().personalBestTime());
    EXPECT_EQ(scenario.stats().personalBestTime()->time_since_epoch(), std::chrono::seconds{5 * kDay / 1000});

    const auto unplayed = Benchmark(benchmark(), {Completeness::Trackable, {}},
                                    {resolvedEntry("entry")}, {});
    EXPECT_FALSE(scenarioNamed(unplayed, "entry").stats().personalBestTime());
}

TEST(BenchmarkEvaluator, UnplayedAndAutoMappableDoNotClaimCandidateFacts) {
    const auto facts = std::vector{benchmarkFact("hash", kDay, 250.0F)};
    const auto evaluated = Benchmark(benchmark(), {Completeness::Trackable, {}},
                                     {autoMappableEntry("entry", "hash")}, facts);
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_EQ(scenario.matchState(), ScenarioMatchState::AutoMappable);
    EXPECT_FALSE(scenario.stats().personalBest());
    EXPECT_FALSE(scenario.attainedTier());
    ASSERT_TRUE(scenario.nextThreshold());
    EXPECT_EQ(scenario.nextThreshold()->tierId.value, "bronze");
    EXPECT_DOUBLE_EQ(scenario.normalizedRank(), 0.0);
}

TEST(BenchmarkEvaluator, TotalPlaytimeSumsEachEntrysPlaytimeWhenIncompleteEntriesShareAHash) {
    auto value = benchmark();
    value.uncategorized.push_back(benchmarkEntry("copy", "Copy", "hash",
        {{"bronze", 100}, {"silver", 200}, {"gold", 300}}));
    const auto facts = std::vector{benchmarkFact("hash", kDay, 250.0F, 60.0F)};
    const auto evaluated = Benchmark(value, {Completeness::Incomplete, {}},
                                     {resolvedEntry("entry"), resolvedEntry("copy")}, facts);
    EXPECT_DOUBLE_EQ(scenarioNamed(evaluated, "entry").stats().playtimeSeconds(), 60.0);
    EXPECT_DOUBLE_EQ(scenarioNamed(evaluated, "copy").stats().playtimeSeconds(), 60.0);
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(), 120.0);
}

TEST(BenchmarkEvaluator, MappedUnavailableAndBrokenLaddersRetainOnlySafeFacts) {
    auto value = benchmark();
    value.uncategorized.push_back(benchmarkEntry("broken", "Broken", "other", {{"bronze", 100.0}}));
    const auto facts = std::vector{benchmarkFact("other", kDay, 250.0F, 30.0F)};
    const auto evaluated = Benchmark(value, {Completeness::Incomplete, {}},
                                     {mappedUnavailableEntry("entry"), resolvedEntry("broken")}, facts);
    const auto &unavailable = scenarioNamed(evaluated, "entry");
    EXPECT_EQ(unavailable.matchState(), ScenarioMatchState::MappedUnavailable);
    EXPECT_FALSE(unavailable.stats().personalBest());
    const auto &broken = scenarioNamed(evaluated, "broken");
    EXPECT_DOUBLE_EQ(*broken.stats().personalBest(), 250.0);
    EXPECT_EQ(broken.stats().runCount(), 1);
    EXPECT_DOUBLE_EQ(broken.stats().playtimeSeconds(), 30.0);
    EXPECT_FALSE(broken.attainedTier());
    EXPECT_FALSE(broken.nextThreshold());
    EXPECT_DOUBLE_EQ(broken.normalizedRank(), 0.0);
}

TEST(BenchmarkEvaluator, MappedUnavailableIgnoresInconsistentRunFacts) {
    const auto facts = std::vector{benchmarkFact("gone", kDay, 250.0F, 30.0F)};
    const auto evaluated = Benchmark(benchmark(), {Completeness::Incomplete, {}},
                                     {mappedUnavailableEntry("entry")}, facts);
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_EQ(scenario.matchState(), ScenarioMatchState::MappedUnavailable);
    EXPECT_FALSE(scenario.stats().personalBest());
    EXPECT_EQ(scenario.stats().runCount(), 0);
    EXPECT_DOUBLE_EQ(scenario.stats().playtimeSeconds(), 0.0);
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(), 0.0);
}

TEST(BenchmarkEvaluator, TraversesEveryDefinitionLevelInOrder) {
    auto value = benchmark();
    Category category{GroupId{"category"}, "Category", {}, {}, {}};
    category.scenarios.push_back(benchmarkEntry("direct", "Direct", std::nullopt, {}));
    Subcategory subgroup{GroupId{"sub"}, "Sub", {}, {}};
    subgroup.scenarios.push_back(benchmarkEntry("nested", "Nested", std::nullopt, {}));
    category.subcategories.push_back(subgroup);
    value.categories.push_back(category);
    const auto evaluated = Benchmark(value, {}, {}, {});
    ASSERT_EQ(evaluated.scenarios().size(), 3U);
    EXPECT_EQ(evaluated.scenarios()[0]->entryId().value, "entry");
    EXPECT_EQ(evaluated.scenarios()[1]->entryId().value, "direct");
    EXPECT_EQ(evaluated.scenarios()[2]->entryId().value, "nested");
    ASSERT_EQ(evaluated.categories().size(), 1U);
    EXPECT_EQ(evaluated.categories()[0]->scenarios()[0]->entryId().value, "direct");
    EXPECT_EQ(evaluated.categories()[0]->subgroups()[0]->scenarios()[0]->entryId().value, "nested");
}

TEST(BenchmarkRank, ComputesGroupSatisfactionAttainedAndCompletedRanks) {
    const auto evaluated = Benchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
                                     hierarchyResolutions(), hierarchyScores({{"d2", 200.0F}, {"b1", 200.0F}}));
    ASSERT_TRUE(evaluated.attainedRank());
    EXPECT_EQ(evaluated.attainedRank()->value, "silver");
    ASSERT_TRUE(evaluated.completedRank());
    EXPECT_EQ(evaluated.completedRank()->value, "silver");
    EXPECT_EQ(groupNamed(evaluated, "direct").satisfiedTier()->value, "gold");
    EXPECT_EQ(groupNamed(evaluated, "nested").satisfiedTier()->value, "silver");
    EXPECT_EQ(groupNamed(evaluated, "first").satisfiedTier()->value, "gold");
    EXPECT_EQ(groupNamed(evaluated, "second").satisfiedTier()->value, "silver");
}

TEST(BenchmarkRank, SuppressesOfficialFieldsWhenIncomplete) {
    const auto evaluated = Benchmark(hierarchyBenchmark(), {Completeness::Incomplete, {}},
                                     hierarchyResolutions(), hierarchyScores());
    EXPECT_FALSE(evaluated.attainedRank());
    EXPECT_FALSE(evaluated.completedRank());
    EXPECT_FALSE(groupNamed(evaluated, "direct").satisfiedTier());
}

TEST(BenchmarkRank, DoesNotLetUnplayedOrBelowFirstThresholdEntriesSatisfyARank) {
    auto resolutions = hierarchyResolutions();
    resolutions[0] = unresolvedEntry("u1");
    const auto unplayed = Benchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
                                    resolutions, hierarchyScores());
    EXPECT_FALSE(unplayed.attainedRank());
    EXPECT_FALSE(unplayed.completedRank());
    const auto belowFirst = Benchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
                                      hierarchyResolutions(), hierarchyScores({{"u1", 99.0F}}));
    EXPECT_FALSE(belowFirst.attainedRank());
    EXPECT_FALSE(belowFirst.completedRank());
}

TEST(BenchmarkNextTier, ReportsOnlyRequirementsThatActuallyBlockTheTier) {
    const auto evaluated = Benchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
                                     hierarchyResolutions(),
                                     hierarchyScores({{"u1", 200.0F}, {"d1", 200.0F},
                                                      {"d2", 100.0F}, {"b1", 200.0F}}));
    ASSERT_TRUE(evaluated.nextTier());
    EXPECT_EQ(evaluated.nextTier()->value, "gold");
    ASSERT_EQ(evaluated.nextTierBlockers().size(), 4U);
    EXPECT_EQ(evaluated.nextTierBlockers()[0].entryId.value, "u1");
    EXPECT_FALSE(evaluated.nextTierBlockers()[0].group);
    EXPECT_EQ(evaluated.nextTierBlockers()[1].group->value, "direct");
    EXPECT_EQ(evaluated.nextTierBlockers()[2].group->value, "direct");
    EXPECT_EQ(evaluated.nextTierBlockers()[3].group->value, "second");
    EXPECT_EQ(evaluated.scenariosAtNextTier(), 3);
}

TEST(BenchmarkNextTier, IsAbsentAtTheHighestAttainedTierAndUsesEntryRequirements) {
    const auto atTop = Benchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
                                 hierarchyResolutions(), hierarchyScores());
    EXPECT_FALSE(atTop.nextTier());
    EXPECT_TRUE(atTop.nextTierBlockers().empty());
    EXPECT_EQ(atTop.scenariosAtNextTier(), 0);
    const auto blocked = Benchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
                                   hierarchyResolutions(), hierarchyScores({{"u2", 200.0F}}));
    ASSERT_EQ(blocked.nextTierBlockers().size(), 1U);
    EXPECT_DOUBLE_EQ(blocked.nextTierBlockers()[0].required, 300.0);
    ASSERT_TRUE(blocked.nextTierBlockers()[0].current);
    EXPECT_DOUBLE_EQ(*blocked.nextTierBlockers()[0].current, 200.0);
}

TEST(BenchmarkAverageRank, IncludesEveryEntryAndIsSuppressedWhenIncomplete) {
    const auto complete = Benchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
                                    hierarchyResolutions(), hierarchyScores({{"u1", 150.0F}}));
    ASSERT_TRUE(complete.averageRank());
    EXPECT_DOUBLE_EQ(*complete.averageRank(), (1.5 + 6.0 * 3.0) / 7.0);
    const auto incomplete = Benchmark(hierarchyBenchmark(), {Completeness::Incomplete, {}},
                                      hierarchyResolutions(), hierarchyScores());
    EXPECT_FALSE(incomplete.averageRank());
}

TEST(BenchmarkAverageRankHistory, EmitsOnePointPerImprovingDayAndEndsAtTheCurrentMean) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 100.0F),
                                   benchmarkFact("one", 100 * kDay + 1, 250.0F),
                                   benchmarkFact("two", 102 * kDay, 200.0F),
                                   benchmarkFact("one", 103 * kDay, 50.0F)};
    const auto evaluated = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                     {resolvedEntry("one"), resolvedEntry("two")}, facts);
    ASSERT_EQ(evaluated.averageRankHistory().size(), 2U);
    EXPECT_EQ(evaluated.averageRankHistory()[0].day.time_since_epoch().count(), 100);
    EXPECT_DOUBLE_EQ(evaluated.averageRankHistory()[0].averageRank, 1.25);
    EXPECT_EQ(evaluated.averageRankHistory()[1].day.time_since_epoch().count(), 102);
    EXPECT_DOUBLE_EQ(evaluated.averageRankHistory()[1].averageRank, 2.25);
    ASSERT_TRUE(evaluated.averageRank());
    EXPECT_DOUBLE_EQ(evaluated.averageRankHistory().back().averageRank, *evaluated.averageRank());
}

TEST(BenchmarkAverageRankHistory, IsEmptyWithoutRunsAndNeverDecreases) {
    const auto resolutions = std::vector{resolvedEntry("one"), resolvedEntry("two")};
    const auto unplayed = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                    resolutions, {});
    EXPECT_TRUE(unplayed.averageRankHistory().empty());

    // A worse run never lowers a personal best, so the line cannot fall however noisy the scores.
    std::vector<RunFact> facts;
    const float pattern[] = {100.0F, 90.0F, 150.0F, 120.0F, 300.0F, 10.0F};
    for (int day = 0; day < 6; ++day)
        facts.push_back(benchmarkFact("one", (100 + day) * kDay, pattern[day]));
    const auto history = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                   resolutions, facts).averageRankHistory();

    ASSERT_EQ(history.size(), 3U);  // only days 100, 102 and 104 improve
    for (std::size_t index = 1; index < history.size(); ++index)
        EXPECT_GE(history[index].averageRank, history[index - 1].averageRank);
}

// Membership is part of the definition the series is rebuilt under: an unplayed scenario joining
// the benchmark reweights every historical point rather than only future ones.
TEST(BenchmarkAverageRankHistory, IsRebuiltWhenAnUnplayedScenarioJoinsTheBenchmark) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 100.0F)};
    const auto resolutions = std::vector{resolvedEntry("one"), resolvedEntry("two")};
    const auto before = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                  resolutions, facts);

    auto widened = twoScenarioBenchmark();
    widened.uncategorized.push_back(benchmarkEntry("three", "Three", std::nullopt,
        {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}}));
    auto widerResolutions = resolutions;
    widerResolutions.push_back(unresolvedEntry("three"));
    const auto after = Benchmark(widened, {Completeness::Trackable, {}},
                                 widerResolutions, facts);

    ASSERT_EQ(before.averageRankHistory().size(), 1U);
    ASSERT_EQ(after.averageRankHistory().size(), 1U);
    EXPECT_DOUBLE_EQ(before.averageRankHistory()[0].averageRank, 1.0 / 2.0);
    EXPECT_DOUBLE_EQ(after.averageRankHistory()[0].averageRank, 1.0 / 3.0);
}

TEST(BenchmarkAverageRankHistory, IgnoresFactsForNonResolvedMappings) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 300.0F)};
    const auto evaluated = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                     {mappedUnavailableEntry("one"), unresolvedEntry("two")}, facts);
    EXPECT_TRUE(evaluated.averageRankHistory().empty());
}

TEST(BenchmarkAverageRankHistory, IsAbsentForIncompleteDefinitionsAndUsesZeroForUnresolvedEntries) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 300.0F),
                                   benchmarkFact("two", 101 * kDay, 300.0F)};
    const auto incomplete = Benchmark(twoScenarioBenchmark(), {Completeness::Incomplete, {}},
                                      {resolvedEntry("one"), resolvedEntry("two")}, facts);
    EXPECT_TRUE(incomplete.averageRankHistory().empty());
    const auto unresolved = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                      {resolvedEntry("one"), unresolvedEntry("two")}, facts);
    ASSERT_EQ(unresolved.averageRankHistory().size(), 1U);
    EXPECT_DOUBLE_EQ(unresolved.averageRankHistory().back().averageRank, 1.5);
}

TEST(BenchmarkRecentRankHistory, EmitsAPointOnEveryPlayDayEvenWithoutImprovement) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 200.0F),
                                   benchmarkFact("one", 101 * kDay, 100.0F)};
    const auto evaluated = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                     {resolvedEntry("one"), resolvedEntry("two")}, facts);
    ASSERT_EQ(evaluated.averageRankHistory().size(), 1U);
    const auto &history = evaluated.recentAverageRankHistory();
    ASSERT_EQ(history.size(), 2U);
    EXPECT_EQ(history[0].day.time_since_epoch().count(), 100);
    EXPECT_DOUBLE_EQ(history[0].averageRank, 1.0);
    EXPECT_EQ(history[1].day.time_since_epoch().count(), 101);
    EXPECT_DOUBLE_EQ(history[1].averageRank, 0.75);
}

TEST(BenchmarkRecentRankHistory, UsesOnlyTheLatestFiveRunsPerEntryAndCanFall) {
    const float scores[] = {300.0F, 100.0F, 100.0F, 100.0F, 100.0F, 100.0F};
    std::vector<RunFact> facts;
    for (int day = 0; day < 6; ++day) facts.push_back(benchmarkFact("hash", (100 + day) * kDay, scores[day]));
    const auto history = Benchmark(benchmark(), {Completeness::Trackable, {}},
                                   {resolvedEntry("entry")}, facts).recentAverageRankHistory();
    ASSERT_EQ(history.size(), 6U);
    EXPECT_DOUBLE_EQ(history[0].averageRank, 3.0);
    EXPECT_DOUBLE_EQ(history[1].averageRank, 2.0);
    EXPECT_DOUBLE_EQ(history[4].averageRank, 1.4);
    // The 300 has left the latest five.
    EXPECT_DOUBLE_EQ(history[5].averageRank, 1.0);
}

TEST(BenchmarkRecentRankHistory, CountsAnUnplayedEntryAsZero) {
    const auto evaluated = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                     {resolvedEntry("one"), unresolvedEntry("two")},
                                     std::vector<RunFact>{benchmarkFact("one", 100 * kDay, 250.0F)});
    ASSERT_EQ(evaluated.recentAverageRankHistory().size(), 1U);
    EXPECT_DOUBLE_EQ(evaluated.recentAverageRankHistory()[0].averageRank, 1.25);
}

TEST(BenchmarkRecentRankHistory, MergesRunsAcrossAnEntrysLinkedHashes) {
    auto definition = benchmark();
    definition.uncategorized.front().hashes = {"hash", "new"};
    std::vector<RunFact> facts;
    for (int day = 1; day <= 6; ++day)
        facts.push_back(benchmarkFact(day % 2 ? "hash" : "new", day * kDay, static_cast<float>(day * 50)));
    const auto history = Benchmark(definition, validateBenchmark(definition), {resolvedEntry("entry")}, facts)
        .recentAverageRankHistory();
    const double expected[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    ASSERT_EQ(history.size(), 6U);
    for (std::size_t index = 0; index < 6; ++index) EXPECT_DOUBLE_EQ(history[index].averageRank, expected[index]);
}

TEST(BenchmarkRecentRankHistory, EndsAtTheMeanOfTheCurrentRecentAverages) {
    const auto definition = twoScenarioBenchmark();
    std::vector<RunFact> facts;
    const float one[] = {120.0F, 260.0F, 90.0F, 310.0F, 150.0F, 40.0F, 220.0F};
    const float two[] = {80.0F, 199.0F, 230.0F};
    for (int run = 0; run < 7; ++run) facts.push_back(benchmarkFact("one", (100 + run) * kDay, one[run]));
    for (int run = 0; run < 3; ++run) facts.push_back(benchmarkFact("two", (103 + 2 * run) * kDay + 5, two[run]));
    std::ranges::sort(facts, [](const RunFact &left, const RunFact &right) {
        return left.run_id.start_time < right.run_id.start_time;
    });
    const auto evaluated = Benchmark(definition, {Completeness::Trackable, {}},
                                     {resolvedEntry("one"), resolvedEntry("two")}, facts);

    double expected = 0.0;
    for (const auto *scenario: evaluated.scenarios())
        expected += scenario->normalized(scenario->stats().recentAverage());
    expected /= 2.0;
    ASSERT_EQ(evaluated.recentAverageRankHistory().size(), 8U);
    EXPECT_NEAR(evaluated.recentAverageRankHistory().back().averageRank, expected, 1e-12);
}

TEST(BenchmarkRecentRankHistory, IsEmptyForAnIncompleteDefinition) {
    const auto evaluated = Benchmark(twoScenarioBenchmark(), {Completeness::Incomplete, {}},
                                     {resolvedEntry("one"), resolvedEntry("two")},
                                     std::vector<RunFact>{benchmarkFact("one", 100 * kDay, 300.0F)});
    EXPECT_TRUE(evaluated.recentAverageRankHistory().empty());
}

TEST(BenchmarkRecentRankHistory, IgnoresDaysWithOnlyRunsOutsideTheBenchmark) {
    const auto facts = std::vector{benchmarkFact("unrelated", 50 * kDay, 300.0F),
                                   benchmarkFact("one", 100 * kDay, 300.0F),
                                   benchmarkFact("unrelated", 101 * kDay, 300.0F)};
    const auto history = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                   {resolvedEntry("one"), resolvedEntry("two")}, facts).recentAverageRankHistory();
    ASSERT_EQ(history.size(), 1U);
    EXPECT_EQ(history[0].day.time_since_epoch().count(), 100);
}

TEST(BenchmarkAttainment, AScoreEqualToAThresholdAttainsThatTier) {
    const auto at = [](float score) {
        return Benchmark(benchmark(), {Completeness::Trackable, {}}, {resolvedEntry("entry")},
                         std::vector<RunFact>{benchmarkFact("hash", kDay, score)});
    };
    const auto exactBenchmark = at(200.0F);
    const auto &exact = scenarioNamed(exactBenchmark, "entry");
    ASSERT_TRUE(exact.attainedTier());
    EXPECT_EQ(exact.attainedTier()->value, "silver");
    ASSERT_TRUE(exact.nextThreshold());
    EXPECT_EQ(exact.nextThreshold()->tierId.value, "gold");

    const auto belowBenchmark = at(199.5F);
    const auto &justBelow = scenarioNamed(belowBenchmark, "entry");
    ASSERT_TRUE(justBelow.attainedTier());
    EXPECT_EQ(justBelow.attainedTier()->value, "bronze");
    ASSERT_TRUE(justBelow.nextThreshold());
    EXPECT_EQ(justBelow.nextThreshold()->tierId.value, "silver");
}

TEST(BenchmarkAverageRank, WeighsEveryScenarioEquallyAndCountsAnUnplayedOneAsZero) {
    const auto evaluated = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                     {resolvedEntry("one"), unresolvedEntry("two")},
                                     std::vector<RunFact>{benchmarkFact("one", 100 * kDay, 250.0F)});
    EXPECT_DOUBLE_EQ(scenarioNamed(evaluated, "one").normalizedRank(), 2.5);
    EXPECT_DOUBLE_EQ(scenarioNamed(evaluated, "two").normalizedRank(), 0.0);
    ASSERT_TRUE(evaluated.averageRank());
    EXPECT_DOUBLE_EQ(*evaluated.averageRank(), (2.5 + 0.0) / 2.0);
}

TEST(BenchmarkAverageRank, AScoreAboveTheTopTierCountsAsTheTopTierWithoutCappingThePersonalBest) {
    const auto evaluated = Benchmark(benchmark(), {Completeness::Trackable, {}},
                                     {resolvedEntry("entry")},
                                     std::vector<RunFact>{benchmarkFact("hash", 100 * kDay, 450.0F)});
    const auto &scenario = scenarioNamed(evaluated, "entry");
    ASSERT_TRUE(scenario.stats().personalBest());
    EXPECT_DOUBLE_EQ(*scenario.stats().personalBest(), 450.0);
    ASSERT_TRUE(scenario.stats().recentAverage());
    EXPECT_DOUBLE_EQ(*scenario.stats().recentAverage(), 450.0);
    EXPECT_DOUBLE_EQ(scenario.normalizedRank(), 3.0);
    ASSERT_TRUE(evaluated.averageRank());
    EXPECT_DOUBLE_EQ(*evaluated.averageRank(), 3.0);
    ASSERT_EQ(evaluated.averageRankHistory().size(), 1U);
    EXPECT_DOUBLE_EQ(evaluated.averageRankHistory().back().averageRank, 3.0);
}

TEST(BenchmarkRecentAverage, UsesExactlyTheLatestFiveRunsAndNeverChangesAttainment) {
    // An early silver PB followed by six weak runs: the recent five sit below bronze.
    std::vector<RunFact> facts{benchmarkFact("hash", 100 * kDay, 250.0F)};
    for (int run = 1; run <= 6; ++run)
        facts.push_back(benchmarkFact("hash", (100 + run) * kDay, static_cast<float>(10 * run)));
    const auto evaluated = Benchmark(benchmark(), {Completeness::Trackable, {}},
                                     {resolvedEntry("entry")}, facts);
    const auto &scenario = scenarioNamed(evaluated, "entry");
    ASSERT_TRUE(scenario.stats().recentAverage());
    EXPECT_DOUBLE_EQ(*scenario.stats().recentAverage(), (20.0 + 30.0 + 40.0 + 50.0 + 60.0) / 5.0);
    EXPECT_EQ(scenario.stats().recentSampleCount(), 5);
    EXPECT_EQ(scenario.stats().runCount(), 7);
    ASSERT_TRUE(scenario.stats().personalBest());
    EXPECT_DOUBLE_EQ(*scenario.stats().personalBest(), 250.0);
    ASSERT_TRUE(scenario.attainedTier());
    EXPECT_EQ(scenario.attainedTier()->value, "silver");
    EXPECT_DOUBLE_EQ(scenario.normalizedRank(), 2.5);
    ASSERT_TRUE(evaluated.attainedRank());
    EXPECT_EQ(evaluated.attainedRank()->value, "silver");
    ASSERT_TRUE(evaluated.averageRank());
    EXPECT_DOUBLE_EQ(*evaluated.averageRank(), 2.5);
}

// Category structure decides official attainment but never the average: the same runs read
// under a recategorized definition change the attained rank and leave the average line alone.
TEST(BenchmarkRank, RecategorizingChangesAttainmentButNotTheAverageRank) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 300.0F),
                                   benchmarkFact("two", 101 * kDay, 100.0F)};
    const auto resolutions = std::vector{resolvedEntry("one"), resolvedEntry("two")};
    const auto separate = Benchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                    resolutions, facts);

    auto grouped = twoScenarioBenchmark();
    Category interchangeable{GroupId{"either"}, "Either", {}, grouped.uncategorized, {}};
    grouped.uncategorized.clear();
    grouped.categories = {interchangeable};
    const auto together = Benchmark(grouped, {Completeness::Trackable, {}}, resolutions, facts);

    ASSERT_TRUE(separate.attainedRank());
    EXPECT_EQ(separate.attainedRank()->value, "bronze");
    ASSERT_TRUE(together.attainedRank());
    EXPECT_EQ(together.attainedRank()->value, "gold");
    // Completion still needs every scenario, whatever the grouping.
    ASSERT_TRUE(separate.completedRank());
    ASSERT_TRUE(together.completedRank());
    EXPECT_EQ(separate.completedRank()->value, "bronze");
    EXPECT_EQ(together.completedRank()->value, "bronze");

    ASSERT_TRUE(separate.averageRank());
    ASSERT_TRUE(together.averageRank());
    EXPECT_DOUBLE_EQ(*separate.averageRank(), 2.0);
    EXPECT_DOUBLE_EQ(*together.averageRank(), 2.0);
    ASSERT_EQ(separate.averageRankHistory().size(), together.averageRankHistory().size());
    for (std::size_t index = 0; index < separate.averageRankHistory().size(); ++index)
        EXPECT_DOUBLE_EQ(separate.averageRankHistory()[index].averageRank,
                         together.averageRankHistory()[index].averageRank);
}

TEST(BenchmarkEvaluator, LinkedHashesCombineChronologicalStatisticsAndHistory) {
    auto definition = benchmark();
    definition.uncategorized.front().hashes = {"hash", "new"};
    std::vector<RunFact> facts;
    for (int day = 1; day <= 6; ++day)
        facts.push_back(benchmarkFact(day % 2 ? "hash" : "new", day * kDay,
                                      static_cast<float>(day * 50), 10.0F));
    facts.push_back(benchmarkFact("unrelated", 7 * kDay, 999.0F, 100.0F));
    const auto evaluated = Benchmark(definition, validateBenchmark(definition),
                                     {resolvedEntry("entry")}, facts);
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_EQ(scenario.stats().runCount(), 6);
    ASSERT_TRUE(scenario.stats().personalBest());
    EXPECT_DOUBLE_EQ(*scenario.stats().personalBest(), 300.0);
    ASSERT_TRUE(scenario.stats().recentAverage());
    EXPECT_DOUBLE_EQ(*scenario.stats().recentAverage(), 200.0);
    EXPECT_EQ(scenario.stats().recentSampleCount(), 5);
    EXPECT_DOUBLE_EQ(scenario.stats().playtimeSeconds(), 60.0);
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(), 60.0);
    ASSERT_TRUE(evaluated.attainedRank());
    EXPECT_EQ(evaluated.attainedRank()->value, "gold");
    ASSERT_EQ(evaluated.averageRankHistory().size(), 6U);
    for (std::size_t index = 0; index < 6; ++index)
        EXPECT_DOUBLE_EQ(evaluated.averageRankHistory()[index].averageRank, .5 * (index + 1));
}

TEST(BenchmarkEvaluator, LinkedHashesSharedByIncompleteEntriesSumEachEntrysPlaytime) {
    auto definition = twoScenarioBenchmark();
    definition.uncategorized[0].hashes.push_back("shared");
    definition.uncategorized[1].hashes.push_back("shared");
    const auto evaluated = Benchmark(definition, {Completeness::Incomplete, {}},
                                     {resolvedEntry("one"), resolvedEntry("two")},
                                     std::vector<RunFact>{
                                         benchmarkFact("one", kDay, 100.0F, 10.0F),
                                         benchmarkFact("shared", 2 * kDay, 200.0F, 20.0F),
                                         benchmarkFact("two", 3 * kDay, 300.0F, 30.0F)});
    EXPECT_DOUBLE_EQ(scenarioNamed(evaluated, "one").stats().playtimeSeconds(), 30.0);
    EXPECT_DOUBLE_EQ(scenarioNamed(evaluated, "two").stats().playtimeSeconds(), 50.0);
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(), 80.0);
}

TEST(BenchmarkEvaluator, LinkedHashRunsWithEqualStartTimesKeepRunFactsOrder) {
    auto definition = benchmark();
    definition.uncategorized.front().hashes = {"hash", "new"};
    // The tie sits on the edge of the five-run window: whichever tied run comes second is the one kept.
    const std::vector facts{benchmarkFact("new", kDay, 100.0F), benchmarkFact("hash", kDay, 200.0F),
                            benchmarkFact("hash", 2 * kDay, 10.0F), benchmarkFact("hash", 3 * kDay, 10.0F),
                            benchmarkFact("hash", 4 * kDay, 10.0F), benchmarkFact("hash", 5 * kDay, 10.0F)};
    const auto evaluated = Benchmark(definition, {Completeness::Incomplete, {}},
                                     {resolvedEntry("entry")}, facts);
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_EQ(scenario.stats().runCount(), 6);
    EXPECT_EQ(scenario.stats().recentSampleCount(), 5);
    ASSERT_TRUE(scenario.stats().recentAverage());
    EXPECT_DOUBLE_EQ(*scenario.stats().recentAverage(), 48.0);
}

TEST(BenchmarkEvaluator, EntryWithNoResolutionProjectsAsUnresolvedWithoutStats) {
    const auto definition = benchmark();
    const auto evaluated = Benchmark(definition, validateBenchmark(definition), {},
                                     std::vector<RunFact>{benchmarkFact("hash", kDay, 250.0F, 30.0F)});
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_EQ(scenario.matchState(), ScenarioMatchState::Unresolved);
    EXPECT_EQ(scenario.stats().runCount(), 0);
    EXPECT_FALSE(scenario.stats().personalBest());
    EXPECT_FALSE(scenario.stats().recentAverage());
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(), 0.0);
}

TEST(BenchmarkAccessors, ExposeTheDefinitionIdTiersAndTheCompletenessItWasBuiltWith) {
    auto definition = benchmark();
    definition.uncategorized.front().thresholds.pop_back();
    const auto completeness = validateBenchmark(definition);
    ASSERT_EQ(completeness.completeness, Completeness::Incomplete);
    ASSERT_FALSE(completeness.issues.empty());

    const auto evaluated = Benchmark(definition, completeness, {resolvedEntry("entry")}, {});
    const auto copy = evaluated;
    for (const auto *value: {&evaluated, &copy}) {
        EXPECT_EQ(value->definition(), definition);
        EXPECT_EQ(value->id(), definition.id);
        EXPECT_EQ(value->tiers(), definition.tiers);
        EXPECT_EQ(value->completeness(), completeness);
    }
}

TEST(BenchmarkScenarioEvaluation, AdmittedThresholdFollowsPerTierRuleWhileLadderIsAllOrNothing) {
    auto definition = benchmark();
    auto &thresholds = definition.uncategorized.front().thresholds;
    thresholds[1].score = -1.0;
    const auto fact = benchmarkFact("hash", kDay, 500.0F);
    const auto evaluated = evaluatedBenchmark(definition, {fact});
    const auto &scenario = scenarioNamed(evaluated, "entry");
    EXPECT_EQ(scenario.thresholdFor(thresholds[0].tierId), thresholds[0].score);
    EXPECT_FALSE(scenario.thresholdFor(thresholds[1].tierId));
    EXPECT_FALSE(scenario.ladder());
    for (std::size_t index = 0; index < definition.tiers.size(); ++index) EXPECT_FALSE(scenario.attains(index));
    EXPECT_DOUBLE_EQ(scenario.normalized(fact.score), 0.0);
}

TEST(BenchmarkScenarioEvaluation, AttainsAndNormalizedAgreeWithTheLadder) {
    const auto definition = benchmark();
    const auto &thresholds = definition.uncategorized.front().thresholds;
    double previous = 0.0;
    for (std::size_t index = 0; index < thresholds.size(); ++index) {
        const auto score = static_cast<float>(thresholds[index].score);
        const auto below = evaluatedBenchmark(definition, {benchmarkFact("hash", kDay, score - 1.0F)});
        const auto exact = evaluatedBenchmark(definition, {benchmarkFact("hash", kDay, score)});
        const auto &scenario = scenarioNamed(exact, "entry");
        ASSERT_TRUE(scenario.ladder());
        ASSERT_EQ(scenario.ladder()->size(), thresholds.size());
        EXPECT_EQ((*scenario.ladder())[index], thresholds[index].score);
        EXPECT_FALSE(scenarioNamed(below, "entry").attains(index));
        EXPECT_TRUE(scenario.attains(index));
        EXPECT_FALSE(scenario.attains(thresholds.size()));
        const auto normalizedBelow = scenario.normalized(score - 1.0F);
        EXPECT_GE(normalizedBelow, previous);
        EXPECT_GT(scenario.normalized(score), normalizedBelow);
        EXPECT_DOUBLE_EQ(scenario.normalized(scenario.stats().personalBest()), scenario.normalizedRank());
        previous = scenario.normalizedRank();
    }
}

TEST(BenchmarkTree, CategoriesExposeDirectScenariosAndSubgroupsAndASubgroupHasNone) {
    const auto definition = hierarchyBenchmark();
    const auto evaluated = evaluatedBenchmark(definition, {});
    const auto categories = evaluated.categories();
    ASSERT_EQ(categories.size(), definition.categories.size());
    for (std::size_t index = 0; index < categories.size(); ++index) {
        const auto &source = definition.categories[index];
        const auto *category = categories[index];
        EXPECT_EQ(category->id(), source.id);
        EXPECT_EQ(category->name(), source.name);
        const auto direct = category->scenarios();
        ASSERT_EQ(direct.size(), source.scenarios.size());
        for (std::size_t i = 0; i < direct.size(); ++i) EXPECT_EQ(direct[i]->entryId(), source.scenarios[i].id);
        const auto subgroups = category->subgroups();
        ASSERT_EQ(subgroups.size(), source.subcategories.size());
        for (std::size_t i = 0; i < subgroups.size(); ++i) {
            EXPECT_EQ(subgroups[i]->id(), source.subcategories[i].id);
            EXPECT_TRUE(subgroups[i]->subgroups().empty());
        }
    }
    const auto scenarios = evaluated.scenarios();
    const auto entries = definition.entries();
    ASSERT_EQ(scenarios.size(), entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        EXPECT_EQ(scenarios[i]->entryId(), entries[i]->id);
        EXPECT_EQ(scenarios[i]->name(), entries[i]->name);
    }
    const auto nested = categories.back()->subgroups().back()->scenarios();
    ASSERT_FALSE(nested.empty());
    EXPECT_EQ(evaluated.scenario(nested.front()->entryId()), nested.front());
    EXPECT_EQ(evaluated.scenario(ScenarioEntryId{"unknown"}), nullptr);
    EXPECT_THROW(static_cast<void>(scenarioNamed(evaluated, "unknown")), std::out_of_range);
}

TEST(BenchmarkTree, GroupColorComesFromTheDefinition) {
    auto definition = hierarchyBenchmark();
    definition.categories.back().color = {12, 34, 56, 255};
    definition.categories.back().subcategories.front().color = {78, 90, 123, 255};
    const auto evaluated = evaluatedBenchmark(definition, {});
    EXPECT_EQ(groupNamed(evaluated, "nested").color(), definition.categories.back().color);
    EXPECT_EQ(groupNamed(evaluated, "first").color(), definition.categories.back().subcategories.front().color);
}

TEST(BenchmarkConstruction, UnplayedEntryAttainsNothingEvenWithAZeroFirstThreshold) {
    auto definition = benchmark();
    auto &first = definition.uncategorized.front().thresholds.front();
    first.score = 0.0;
    const auto unplayed = evaluatedBenchmark(definition, {});
    const auto &scenario = scenarioNamed(unplayed, "entry");
    EXPECT_FALSE(scenario.attains(0));
    EXPECT_FALSE(scenario.attainedTier());
    EXPECT_EQ(scenario.nextThreshold(), first);

    const auto played = evaluatedBenchmark(definition, {benchmarkFact("hash", kDay, 0.0F)});
    const auto &playedScenario = scenarioNamed(played, "entry");
    EXPECT_TRUE(playedScenario.attains(0));
    EXPECT_EQ(playedScenario.attainedTier(), first.tierId);
}

TEST(BenchmarkRankLevel, IncompleteBuildsTheTreeStatsAndPlaytimeButNoRankResults) {
    auto facts = hierarchyScores();
    for (auto &fact: facts) fact.duration_seconds = 10.0F;
    facts.push_back(benchmarkFact("b1", facts.back().run_id.start_time + kDay, 300.0F, 20.0F));
    const auto evaluated = Benchmark(hierarchyBenchmark(), {Completeness::Incomplete, {}},
                                     hierarchyResolutions(), facts);
    ASSERT_EQ(evaluated.categories().size(), 2U);
    const auto subgroups = evaluated.categories().back()->subgroups();
    ASSERT_EQ(subgroups.size(), 2U);
    const auto nested = subgroups.back()->scenarios();
    ASSERT_EQ(nested.size(), 1U);
    EXPECT_EQ(evaluated.scenario(ScenarioEntryId{"b1"}), nested.front());
    EXPECT_EQ(nested.front()->stats().runCount(), 2);
    double playtime = 0.0;
    for (const auto *scenario: evaluated.scenarios()) {
        EXPECT_GT(scenario->stats().runCount(), 0);
        playtime += scenario->stats().playtimeSeconds();
    }
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(), playtime);
    EXPECT_FALSE(evaluated.rollingPlaytime(3).empty());
    EXPECT_EQ(evaluated.latestRunDay(), facts.back().run_id.startDay());
    for (const auto *group: evaluated.categories()) {
        EXPECT_FALSE(group->satisfiedTier());
        for (const auto *subgroup: group->subgroups()) EXPECT_FALSE(subgroup->satisfiedTier());
    }

    const auto empty = Benchmark(BenchmarkDefinition{.tiers = ladder3()}, {Completeness::Trackable, {}}, {}, {});
    EXPECT_TRUE(empty.scenarios().empty());
    EXPECT_TRUE(empty.uncategorized().empty());
    EXPECT_TRUE(empty.categories().empty());
    for (const auto *value: {&evaluated, &empty}) {
        EXPECT_FALSE(value->attainedRank());
        EXPECT_FALSE(value->completedRank());
        EXPECT_FALSE(value->nextTier());
        EXPECT_TRUE(value->nextTierBlockers().empty());
        EXPECT_EQ(value->scenariosAtNextTier(), 0);
        EXPECT_FALSE(value->averageRank());
        EXPECT_TRUE(value->averageRankHistory().empty());
        EXPECT_TRUE(value->recentAverageRankHistory().empty());
    }
}

namespace {
    using PlaytimeSeries = std::vector<std::pair<std::chrono::sys_days, double>>;

    double sumPlaytime(const PlaytimeSeries &series) {
        return std::accumulate(series.begin(), series.end(), 0.0,
                               [](double sum, const auto &point) { return sum + point.second; });
    }

    void expectHistory(const std::vector<AverageRankPoint> &actual, const std::vector<AverageRankPoint> &expected) {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t index = 0; index < actual.size(); ++index) {
            EXPECT_EQ(actual[index].day, expected[index].day);
            EXPECT_DOUBLE_EQ(actual[index].averageRank, expected[index].averageRank);
        }
    }
}

TEST(BenchmarkMembership, TotalRollingAndLatestRunDayCountOnlyResolvedEntriesOncePerEntry) {
    auto definition = twoScenarioBenchmark();
    definition.uncategorized[1].hashes.push_back("one");
    definition.uncategorized.push_back(benchmarkEntry("unavailable", "Unavailable", "gone",
                                                     {{"bronze", 100}, {"silver", 200}, {"gold", 300}}));
    // All runs are post-epoch so the rolling series and the total use the same timestamps.
    const std::vector facts{benchmarkFact("two", kDay, 250.0F, 11.0F),
                            benchmarkFact("one", 2 * kDay, 100.0F, 23.0F),
                            benchmarkFact("gone", 3 * kDay, 350.0F, 51.0F),
                            benchmarkFact("unrelated", 4 * kDay, 450.0F, 61.0F)};
    const auto evaluated = evaluatedBenchmark(definition, facts,
        {resolvedEntry("one"), resolvedEntry("two"), mappedUnavailableEntry("unavailable")});
    const auto &one = scenarioNamed(evaluated, "one");
    const auto &two = scenarioNamed(evaluated, "two");
    const auto &unavailable = scenarioNamed(evaluated, "unavailable");
    EXPECT_EQ(one.stats().runCount(), 1);
    EXPECT_EQ(two.stats().runCount(), 2);
    EXPECT_EQ(unavailable.stats().runCount(), 0);
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(), facts[0].duration_seconds + 2.0 * facts[1].duration_seconds);
    EXPECT_DOUBLE_EQ(evaluated.totalPlaytimeSeconds(),
                     one.stats().playtimeSeconds() + two.stats().playtimeSeconds() + unavailable.stats().playtimeSeconds());
    const auto rolling = evaluated.rollingPlaytime(1);
    EXPECT_DOUBLE_EQ(sumPlaytime(rolling), evaluated.totalPlaytimeSeconds());
    ASSERT_EQ(rolling.size(), 2U);
    EXPECT_EQ(rolling.back().first, facts[1].run_id.startDay());
    EXPECT_EQ(evaluated.latestRunDay(), facts[1].run_id.startDay());
}

TEST(BenchmarkPlaytime, TotalCountsAPreEpochRunButRollingPlaytimeAndLatestRunDaySkipIt) {
    std::vector facts{benchmarkFact("hash", -kDay, 200.0F, 10.0F), benchmarkFact("hash", 0, 250.0F, 20.0F)};
    const double earlyPlaytime = facts[0].duration_seconds + facts[1].duration_seconds;
    const auto early = evaluatedBenchmark(benchmark(), facts);
    EXPECT_DOUBLE_EQ(early.totalPlaytimeSeconds(), earlyPlaytime);
    EXPECT_TRUE(early.rollingPlaytime(1).empty());
    EXPECT_FALSE(early.latestRunDay());

    facts.push_back(benchmarkFact("hash", kDay, 300.0F, 30.0F));
    const auto mixed = evaluatedBenchmark(benchmark(), facts);
    EXPECT_DOUBLE_EQ(mixed.totalPlaytimeSeconds(), earlyPlaytime + facts.back().duration_seconds);
    EXPECT_DOUBLE_EQ(sumPlaytime(mixed.rollingPlaytime(1)), facts.back().duration_seconds);
    EXPECT_EQ(mixed.latestRunDay(), facts.back().run_id.startDay());
}

TEST(BenchmarkRollingPlaytime, AgreesWithTheProfileSeriesAndWithHandDerivedValues) {
    constexpr float firstDuration = 33.0F;
    constexpr float secondDuration = 52.0F;
    constexpr float lastDuration = 43.0F;
    const std::vector facts{benchmarkFact("hash", 10 * kDay, 200.0F, firstDuration),
                            benchmarkFact("hash", 11 * kDay, 250.0F, secondDuration),
                            benchmarkFact("hash", 15 * kDay, 300.0F, lastDuration)};
    UserProfile profile;
    for (const auto &fact: facts)
        profile.addRun(makeRun("hash", fact.run_id.start_time, fact.score, 0, 0, fact.duration_seconds));
    const auto evaluated = evaluatedBenchmark(benchmark(), facts);
    const std::vector scenarios{facts.front().run_id.scenario_id};
    for (const int window: {1, 3, 7})
        EXPECT_EQ(evaluated.rollingPlaytime(window), profile.getRollingTimeAverage(scenarios, window));

    using std::chrono::days;
    const auto firstDay = facts.front().run_id.startDay();
    const PlaytimeSeries expected{
        {firstDay, firstDuration},
        {firstDay + days{1}, (firstDuration + secondDuration) / 2.0},
        {firstDay + days{2}, (firstDuration + secondDuration) / 3.0},
        {firstDay + days{3}, secondDuration / 3.0},
        {firstDay + days{5}, lastDuration / 3.0},
    };
    const auto series = evaluated.rollingPlaytime(3);
    ASSERT_EQ(series.size(), expected.size());
    for (std::size_t index = 0; index < series.size(); ++index) {
        EXPECT_EQ(series[index].first, expected[index].first);
        EXPECT_DOUBLE_EQ(series[index].second, expected[index].second);
    }
}

TEST(BenchmarkCopy, ACopyIsIndependentOfItsSource) {
    auto facts = hierarchyScores({{"u1", 200.0F}});
    for (auto &fact: facts) fact.duration_seconds = 10.0F;
    const ScenarioEntryId nestedId{"a1"};
    auto assigned = evaluatedBenchmark(benchmark(), {});
    const auto copy = [&] {
        const auto source = evaluatedBenchmark(hierarchyBenchmark(), facts);
        auto result = source;
        EXPECT_NE(result.scenario(nestedId), source.scenario(nestedId));
        assigned = source;
        return result;
    }();

    ASSERT_NE(copy.scenario(nestedId), nullptr);
    EXPECT_EQ(copy.scenario(nestedId)->entryId(), nestedId);
    ASSERT_TRUE(copy.scenario(nestedId)->stats().personalBest());
    EXPECT_DOUBLE_EQ(*copy.scenario(nestedId)->stats().personalBest(), facts[4].score);
    const auto categories = copy.categories();
    ASSERT_EQ(categories.size(), 2U);
    ASSERT_EQ(categories.front()->scenarios().size(), 2U);
    EXPECT_EQ(categories.front()->scenarios().front(), copy.scenario(ScenarioEntryId{"d1"}));
    ASSERT_EQ(categories.back()->subgroups().size(), 2U);
    ASSERT_FALSE(categories.back()->subgroups().front()->scenarios().empty());
    EXPECT_EQ(categories.back()->subgroups().front()->scenarios().front(), copy.scenario(nestedId));
    EXPECT_EQ(copy.attainedRank(), copy.tiers()[1].id);
    EXPECT_EQ(copy.completedRank(), copy.tiers()[1].id);
    EXPECT_EQ(copy.nextTier(), copy.tiers()[2].id);
    const double expectedRank = (2.0 + 6 * 3.0) / 7.0;
    ASSERT_EQ(copy.averageRankHistory().size(), 1U);
    ASSERT_EQ(copy.recentAverageRankHistory().size(), 1U);
    EXPECT_DOUBLE_EQ(copy.averageRankHistory().front().averageRank, expectedRank);
    EXPECT_DOUBLE_EQ(copy.recentAverageRankHistory().front().averageRank, expectedRank);
    EXPECT_DOUBLE_EQ(sumPlaytime(copy.rollingPlaytime(3)), copy.totalPlaytimeSeconds());

    const auto expectSameResults = [&](const Benchmark &other) {
        ASSERT_NE(other.scenario(nestedId), nullptr);
        EXPECT_EQ(other.scenario(nestedId)->stats(), copy.scenario(nestedId)->stats());
        EXPECT_EQ(other.attainedRank(), copy.attainedRank());
        EXPECT_EQ(other.completedRank(), copy.completedRank());
        EXPECT_EQ(other.nextTier(), copy.nextTier());
        expectHistory(other.averageRankHistory(), copy.averageRankHistory());
        expectHistory(other.recentAverageRankHistory(), copy.recentAverageRankHistory());
        EXPECT_EQ(other.rollingPlaytime(3), copy.rollingPlaytime(3));
    };
    expectSameResults(assigned);
    const auto moved = std::move(assigned);
    expectSameResults(moved);
}

namespace {
    void expectProjectedGroup(const GroupProjection &projection, const GroupEvaluation &group) {
        EXPECT_EQ(projection.id, group.id());
        EXPECT_EQ(projection.name, group.name());
        EXPECT_EQ(projection.satisfiedTier, group.satisfiedTier());
        const auto scenarios = group.scenarios();
        ASSERT_EQ(projection.scenarios.size(), scenarios.size());
        for (std::size_t i = 0; i < scenarios.size(); ++i) EXPECT_EQ(projection.scenarios[i], scenarios[i]->entryId());
        const auto subgroups = group.subgroups();
        ASSERT_EQ(projection.subgroups.size(), subgroups.size());
        for (std::size_t i = 0; i < subgroups.size(); ++i) expectProjectedGroup(projection.subgroups[i], *subgroups[i]);
    }

    void expectProjection(const BenchmarkProjection &projection, const Benchmark &evaluated,
                          const PlaytimeSeries &series) {
        EXPECT_EQ(projection.benchmarkId, evaluated.id());
        EXPECT_EQ(projection.completeness, evaluated.completeness().completeness);
        EXPECT_EQ(projection.tiers, evaluated.tiers());
        EXPECT_EQ(projection.attainedRank, evaluated.attainedRank());
        EXPECT_EQ(projection.completedRank, evaluated.completedRank());
        EXPECT_EQ(projection.nextTier, evaluated.nextTier());
        EXPECT_EQ(projection.scenariosAtNextTier, evaluated.scenariosAtNextTier());
        EXPECT_EQ(projection.averageRank, evaluated.averageRank());
        EXPECT_DOUBLE_EQ(projection.totalPlaytimeSeconds, evaluated.totalPlaytimeSeconds());
        EXPECT_EQ(projection.rollingPlaytime, series);
        expectHistory(projection.averageRankHistory, evaluated.averageRankHistory());
        expectHistory(projection.recentAverageRankHistory, evaluated.recentAverageRankHistory());
        const auto &blockers = evaluated.nextTierBlockers();
        ASSERT_EQ(projection.nextTierBlockers.size(), blockers.size());
        for (std::size_t i = 0; i < blockers.size(); ++i) {
            const auto &projected = projection.nextTierBlockers[i];
            EXPECT_EQ(projected.entryId, blockers[i].entryId);
            EXPECT_EQ(projected.group, blockers[i].group);
            EXPECT_DOUBLE_EQ(projected.required, blockers[i].required);
            EXPECT_EQ(projected.current, blockers[i].current);
        }

        const auto scenarios = evaluated.scenarios();
        ASSERT_EQ(projection.scenarios.size(), scenarios.size());
        for (std::size_t i = 0; i < scenarios.size(); ++i) {
            const auto &projected = projection.scenarios[i];
            const auto &scenario = *scenarios[i];
            const auto &stats = scenario.stats();
            EXPECT_EQ(projected.entryId, scenario.entryId());
            EXPECT_EQ(projected.name, scenario.name());
            EXPECT_EQ(projected.matchState, scenario.matchState());
            EXPECT_EQ(projected.attainedTier, scenario.attainedTier());
            EXPECT_EQ(projected.nextThreshold, scenario.nextThreshold());
            EXPECT_DOUBLE_EQ(projected.normalizedRank, scenario.normalizedRank());
            EXPECT_EQ(projected.personalBest, stats.personalBest());
            EXPECT_EQ(projected.personalBestTime, stats.personalBestTime());
            EXPECT_EQ(projected.recentAverage, stats.recentAverage());
            EXPECT_EQ(projected.recentSampleCount, stats.recentSampleCount());
            EXPECT_EQ(projected.runCount, stats.runCount());
            EXPECT_DOUBLE_EQ(projected.playtimeSeconds, stats.playtimeSeconds());
        }
        const auto uncategorized = evaluated.uncategorized();
        ASSERT_EQ(projection.uncategorized.size(), uncategorized.size());
        for (std::size_t i = 0; i < uncategorized.size(); ++i) EXPECT_EQ(projection.uncategorized[i], uncategorized[i]->entryId());
        const auto categories = evaluated.categories();
        ASSERT_EQ(projection.categories.size(), categories.size());
        for (std::size_t i = 0; i < categories.size(); ++i) expectProjectedGroup(projection.categories[i], *categories[i]);
    }
}

// TODO(2026-11-19): Delete with Cleanup when the temporary adapter is removed.
TEST(EvaluateBenchmarkAdapter, AnswersTheSameAsTheBenchmarkItWraps) {
    const auto definition = hierarchyBenchmark();
    const auto resolutions = hierarchyResolutions();
    const auto completeness = validateBenchmark(definition);
    auto facts = hierarchyScores({{"b1", 200.0F}});
    facts.front().duration_seconds = 15.0F;
    const auto evaluated = evaluatedBenchmark(definition, facts, resolutions);
    ASSERT_FALSE(evaluated.nextTierBlockers().empty());
    ASSERT_FALSE(evaluated.averageRankHistory().empty());
    const PlaytimeSeries series{{facts.front().run_id.startDay() + std::chrono::days{10}, 123.0}};
    expectProjection(toProjection(evaluated, series), evaluated, series);
    expectProjection(evaluateBenchmark(definition, completeness, resolutions, facts, series), evaluated, series);
}
