#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include "benchmark_builders.h"
#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_evaluator.h"

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
    Benchmark benchmark() {
        Benchmark value{BenchmarkId{"bench"}, "Bench", ladder3(), {}, {}};
        value.uncategorized.push_back(benchmarkEntry("entry", "Alpha", "hash",
            {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}}));
        return value;
    }
    constexpr long long kDay = 86'400'000LL;

    Benchmark hierarchyBenchmark() {
        const std::initializer_list<std::pair<std::string, double> > rungs{
            {"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}};
        Benchmark value{BenchmarkId{"hierarchy"}, "Hierarchy", ladder3(), {}, {}};
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
        return {resolvedEntry("u1", "u1"), resolvedEntry("u2", "u2"),
                resolvedEntry("d1", "d1"), resolvedEntry("d2", "d2"),
                resolvedEntry("a1", "a1"), resolvedEntry("a2", "a2"),
                resolvedEntry("b1", "b1")};
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

    const GroupProjection &groupNamed(const BenchmarkProjection &projection, const std::string &id) {
        for (const auto &category: projection.categories) {
            if (category.id.value == id) return category;
            for (const auto &subcategory: category.subgroups)
                if (subcategory.id.value == id) return subcategory;
        }
        throw std::out_of_range("unknown group");
    }

    Benchmark twoScenarioBenchmark() {
        Benchmark value{BenchmarkId{"two"}, "Two", ladder3(), {}, {}};
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
}

TEST(BenchmarkNormalization, ZeroFirstThresholdDistinguishesPlayedFromUnplayed) {
    const auto thresholds = steps({{"bronze", 0}, {"silver", 200}, {"gold", 300}});
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(normalizedTierValue(ladder3(), thresholds, 100.0), 1.5);
}

TEST(BenchmarkEvaluator, ProjectsPersonalBestRecentFiveAndTierProgress) {
    std::vector<RunFact> facts;
    for (int score = 10; score <= 60; score += 10)
        facts.push_back(benchmarkFact("hash", kDay * score, static_cast<float>(score), 3.0F));
    const auto projection = evaluateBenchmark(benchmark(), {Completeness::Trackable, {}},
                                              {resolvedEntry("entry", "hash")}, facts, {});
    const auto &scenario = scenarioNamed(projection, "entry");
    EXPECT_DOUBLE_EQ(*scenario.personalBest, 60.0);
    EXPECT_DOUBLE_EQ(*scenario.recentAverage, 40.0);
    EXPECT_EQ(scenario.recentSampleCount, 5);
    EXPECT_EQ(scenario.runCount, 6);
    EXPECT_FALSE(scenario.attainedTier);
    ASSERT_TRUE(scenario.nextThreshold);
    EXPECT_EQ(scenario.nextThreshold->tierId.value, "bronze");
    EXPECT_DOUBLE_EQ(scenario.playtimeSeconds, 18.0);
    EXPECT_DOUBLE_EQ(projection.totalPlaytimeSeconds, 18.0);
}

TEST(BenchmarkEvaluator, PersonalBestTimeIsTheEarliestRunThatScoredIt) {
    // Out of order on purpose: the later tie (day 9) is listed first.
    const auto facts = std::vector{benchmarkFact("hash", 9 * kDay, 80.0F), benchmarkFact("hash", 2 * kDay, 50.0F),
                                   benchmarkFact("hash", 5 * kDay, 80.0F), benchmarkFact("hash", 7 * kDay, 70.0F)};
    const auto projection = evaluateBenchmark(benchmark(), {Completeness::Trackable, {}},
                                              {resolvedEntry("entry", "hash")}, facts, {});
    const auto &scenario = scenarioNamed(projection, "entry");
    EXPECT_DOUBLE_EQ(*scenario.personalBest, 80.0);
    ASSERT_TRUE(scenario.personalBestTime);
    EXPECT_EQ(scenario.personalBestTime->time_since_epoch(), std::chrono::seconds{5 * kDay / 1000});

    const auto unplayed = evaluateBenchmark(benchmark(), {Completeness::Trackable, {}},
                                            {resolvedEntry("entry", "hash")}, {}, {});
    EXPECT_FALSE(scenarioNamed(unplayed, "entry").personalBestTime);
}

TEST(BenchmarkEvaluator, UnplayedAndAutoMappableDoNotClaimCandidateFacts) {
    const auto facts = std::vector{benchmarkFact("hash", kDay, 250.0F)};
    const auto projection = evaluateBenchmark(benchmark(), {Completeness::Trackable, {}},
                                              {autoMappableEntry("entry", "hash")}, facts, {});
    const auto &scenario = scenarioNamed(projection, "entry");
    EXPECT_EQ(scenario.matchState, ScenarioMatchState::AutoMappable);
    EXPECT_FALSE(scenario.personalBest);
    EXPECT_FALSE(scenario.attainedTier);
    ASSERT_TRUE(scenario.nextThreshold);
    EXPECT_EQ(scenario.nextThreshold->tierId.value, "bronze");
    EXPECT_DOUBLE_EQ(scenario.normalizedRank, 0.0);
}

TEST(BenchmarkEvaluator, CountsAResolvedHashOnlyOnceForTotalPlaytime) {
    auto value = benchmark();
    value.uncategorized.push_back(benchmarkEntry("copy", "Copy", "hash",
        {{"bronze", 100}, {"silver", 200}, {"gold", 300}}));
    const auto facts = std::vector{benchmarkFact("hash", kDay, 250.0F, 60.0F)};
    const auto projection = evaluateBenchmark(value, {Completeness::Incomplete, {}},
        {resolvedEntry("entry", "hash"), resolvedEntry("copy", "hash")}, facts, {});
    EXPECT_DOUBLE_EQ(scenarioNamed(projection, "entry").playtimeSeconds, 60.0);
    EXPECT_DOUBLE_EQ(scenarioNamed(projection, "copy").playtimeSeconds, 60.0);
    EXPECT_DOUBLE_EQ(projection.totalPlaytimeSeconds, 60.0);
}

TEST(BenchmarkEvaluator, MappedUnavailableAndBrokenLaddersRetainOnlySafeFacts) {
    auto value = benchmark();
    value.uncategorized.push_back(benchmarkEntry("broken", "Broken", "other", {{"bronze", 100.0}}));
    const auto facts = std::vector{benchmarkFact("other", kDay, 250.0F, 30.0F)};
    const auto projection = evaluateBenchmark(value, {Completeness::Incomplete, {}},
        {mappedUnavailableEntry("entry", "gone"), resolvedEntry("broken", "other")}, facts, {});
    const auto &unavailable = scenarioNamed(projection, "entry");
    EXPECT_EQ(unavailable.matchState, ScenarioMatchState::MappedUnavailable);
    EXPECT_FALSE(unavailable.personalBest);
    const auto &broken = scenarioNamed(projection, "broken");
    EXPECT_DOUBLE_EQ(*broken.personalBest, 250.0);
    EXPECT_EQ(broken.runCount, 1);
    EXPECT_DOUBLE_EQ(broken.playtimeSeconds, 30.0);
    EXPECT_FALSE(broken.attainedTier);
    EXPECT_FALSE(broken.nextThreshold);
    EXPECT_DOUBLE_EQ(broken.normalizedRank, 0.0);
}

TEST(BenchmarkEvaluator, MappedUnavailableIgnoresInconsistentRunFacts) {
    const auto facts = std::vector{benchmarkFact("gone", kDay, 250.0F, 30.0F)};
    const auto projection = evaluateBenchmark(benchmark(), {Completeness::Incomplete, {}},
                                              {mappedUnavailableEntry("entry", "gone")}, facts, {});
    const auto &scenario = scenarioNamed(projection, "entry");
    EXPECT_EQ(scenario.matchState, ScenarioMatchState::MappedUnavailable);
    EXPECT_FALSE(scenario.personalBest);
    EXPECT_EQ(scenario.runCount, 0);
    EXPECT_DOUBLE_EQ(scenario.playtimeSeconds, 0.0);
    EXPECT_DOUBLE_EQ(projection.totalPlaytimeSeconds, 0.0);
}

TEST(BenchmarkEvaluator, TraversesEveryDefinitionLevelInOrder) {
    auto value = benchmark();
    Category category{GroupId{"category"}, "Category", {}, {}, {}};
    category.scenarios.push_back(benchmarkEntry("direct", "Direct", std::nullopt, {}));
    Subcategory subgroup{GroupId{"sub"}, "Sub", {}, {}};
    subgroup.scenarios.push_back(benchmarkEntry("nested", "Nested", std::nullopt, {}));
    category.subcategories.push_back(subgroup);
    value.categories.push_back(category);
    const auto projection = evaluateBenchmark(value, {}, {}, {}, {});
    ASSERT_EQ(projection.scenarios.size(), 3U);
    EXPECT_EQ(projection.scenarios[0].entryId.value, "entry");
    EXPECT_EQ(projection.scenarios[1].entryId.value, "direct");
    EXPECT_EQ(projection.scenarios[2].entryId.value, "nested");
    ASSERT_EQ(projection.categories.size(), 1U);
    EXPECT_EQ(projection.categories[0].scenarios[0].value, "direct");
    EXPECT_EQ(projection.categories[0].subgroups[0].scenarios[0].value, "nested");
}

TEST(BenchmarkRank, ComputesGroupSatisfactionAttainedAndCompletedRanks) {
    const auto projection = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
        hierarchyResolutions(), hierarchyScores({{"d2", 200.0F}, {"b1", 200.0F}}), {});
    ASSERT_TRUE(projection.attainedRank);
    EXPECT_EQ(projection.attainedRank->value, "silver");
    ASSERT_TRUE(projection.completedRank);
    EXPECT_EQ(projection.completedRank->value, "silver");
    EXPECT_EQ(groupNamed(projection, "direct").satisfiedTier->value, "gold");
    EXPECT_EQ(groupNamed(projection, "nested").satisfiedTier->value, "silver");
    EXPECT_EQ(groupNamed(projection, "first").satisfiedTier->value, "gold");
    EXPECT_EQ(groupNamed(projection, "second").satisfiedTier->value, "silver");
}

TEST(BenchmarkRank, SuppressesOfficialFieldsWhenIncomplete) {
    const auto projection = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Incomplete, {}},
        hierarchyResolutions(), hierarchyScores(), {});
    EXPECT_FALSE(projection.attainedRank);
    EXPECT_FALSE(projection.completedRank);
    EXPECT_FALSE(groupNamed(projection, "direct").satisfiedTier);
}

TEST(BenchmarkRank, DoesNotLetUnplayedOrBelowFirstThresholdEntriesSatisfyARank) {
    auto resolutions = hierarchyResolutions();
    resolutions[0] = unresolvedEntry("u1");
    const auto unplayed = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
        resolutions, hierarchyScores(), {});
    EXPECT_FALSE(unplayed.attainedRank);
    EXPECT_FALSE(unplayed.completedRank);
    const auto belowFirst = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
        hierarchyResolutions(), hierarchyScores({{"u1", 99.0F}}), {});
    EXPECT_FALSE(belowFirst.attainedRank);
    EXPECT_FALSE(belowFirst.completedRank);
}

TEST(BenchmarkNextTier, ReportsOnlyRequirementsThatActuallyBlockTheTier) {
    const auto projection = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
        hierarchyResolutions(), hierarchyScores({{"u1", 200.0F}, {"d1", 200.0F}, {"d2", 100.0F},
                                                  {"b1", 200.0F}}), {});
    ASSERT_TRUE(projection.nextTier);
    EXPECT_EQ(projection.nextTier->value, "gold");
    ASSERT_EQ(projection.nextTierBlockers.size(), 4U);
    EXPECT_EQ(projection.nextTierBlockers[0].entryId.value, "u1");
    EXPECT_FALSE(projection.nextTierBlockers[0].group);
    EXPECT_EQ(projection.nextTierBlockers[1].group->value, "direct");
    EXPECT_EQ(projection.nextTierBlockers[2].group->value, "direct");
    EXPECT_EQ(projection.nextTierBlockers[3].group->value, "second");
    EXPECT_EQ(projection.scenariosAtNextTier, 3);
}

TEST(BenchmarkNextTier, IsAbsentAtTheHighestAttainedTierAndUsesEntryRequirements) {
    const auto atTop = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
        hierarchyResolutions(), hierarchyScores(), {});
    EXPECT_FALSE(atTop.nextTier);
    EXPECT_TRUE(atTop.nextTierBlockers.empty());
    EXPECT_EQ(atTop.scenariosAtNextTier, 0);
    const auto blocked = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
        hierarchyResolutions(), hierarchyScores({{"u2", 200.0F}}), {});
    ASSERT_EQ(blocked.nextTierBlockers.size(), 1U);
    EXPECT_DOUBLE_EQ(blocked.nextTierBlockers[0].required, 300.0);
    ASSERT_TRUE(blocked.nextTierBlockers[0].current);
    EXPECT_DOUBLE_EQ(*blocked.nextTierBlockers[0].current, 200.0);
}

TEST(BenchmarkAverageRank, IncludesEveryEntryAndIsSuppressedWhenIncomplete) {
    const auto complete = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Trackable, {}},
        hierarchyResolutions(), hierarchyScores({{"u1", 150.0F}}), {});
    ASSERT_TRUE(complete.averageRank);
    EXPECT_DOUBLE_EQ(*complete.averageRank, (1.5 + 6.0 * 3.0) / 7.0);
    const auto incomplete = evaluateBenchmark(hierarchyBenchmark(), {Completeness::Incomplete, {}},
        hierarchyResolutions(), hierarchyScores(), {});
    EXPECT_FALSE(incomplete.averageRank);
}

TEST(BenchmarkAverageRankHistory, EmitsOnePointPerImprovingDayAndEndsAtTheCurrentMean) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 100.0F),
                                   benchmarkFact("one", 100 * kDay + 1, 250.0F),
                                   benchmarkFact("two", 102 * kDay, 200.0F),
                                   benchmarkFact("one", 103 * kDay, 50.0F)};
    const auto projection = evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
        {resolvedEntry("one", "one"), resolvedEntry("two", "two")}, facts, {});
    ASSERT_EQ(projection.averageRankHistory.size(), 2U);
    EXPECT_EQ(projection.averageRankHistory[0].day.time_since_epoch().count(), 100);
    EXPECT_DOUBLE_EQ(projection.averageRankHistory[0].averageRank, 1.25);
    EXPECT_EQ(projection.averageRankHistory[1].day.time_since_epoch().count(), 102);
    EXPECT_DOUBLE_EQ(projection.averageRankHistory[1].averageRank, 2.25);
    ASSERT_TRUE(projection.averageRank);
    EXPECT_DOUBLE_EQ(projection.averageRankHistory.back().averageRank, *projection.averageRank);
}

TEST(BenchmarkAverageRankHistory, IsEmptyWithoutRunsAndNeverDecreases) {
    const auto resolutions = std::vector{resolvedEntry("one", "one"), resolvedEntry("two", "two")};
    EXPECT_TRUE(evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                  resolutions, {}, {}).averageRankHistory.empty());

    // A worse run never lowers a personal best, so the line cannot fall however noisy the scores.
    std::vector<RunFact> facts;
    const float pattern[] = {100.0F, 90.0F, 150.0F, 120.0F, 300.0F, 10.0F};
    for (int day = 0; day < 6; ++day)
        facts.push_back(benchmarkFact("one", (100 + day) * kDay, pattern[day]));
    const auto history = evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                           resolutions, facts, {}).averageRankHistory;

    ASSERT_EQ(history.size(), 3U);  // only days 100, 102 and 104 improve
    for (std::size_t index = 1; index < history.size(); ++index)
        EXPECT_GE(history[index].averageRank, history[index - 1].averageRank);
}

// Membership is part of the definition the series is rebuilt under: an unplayed scenario joining
// the benchmark reweights every historical point rather than only future ones.
TEST(BenchmarkAverageRankHistory, IsRebuiltWhenAnUnplayedScenarioJoinsTheBenchmark) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 100.0F)};
    const auto resolutions = std::vector{resolvedEntry("one", "one"), resolvedEntry("two", "two")};
    const auto before = evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                          resolutions, facts, {});

    auto widened = twoScenarioBenchmark();
    widened.uncategorized.push_back(benchmarkEntry("three", "Three", std::nullopt,
        {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}}));
    auto widerResolutions = resolutions;
    widerResolutions.push_back(unresolvedEntry("three"));
    const auto after = evaluateBenchmark(widened, {Completeness::Trackable, {}},
                                         widerResolutions, facts, {});

    ASSERT_EQ(before.averageRankHistory.size(), 1U);
    ASSERT_EQ(after.averageRankHistory.size(), 1U);
    EXPECT_DOUBLE_EQ(before.averageRankHistory[0].averageRank, 1.0 / 2.0);
    EXPECT_DOUBLE_EQ(after.averageRankHistory[0].averageRank, 1.0 / 3.0);
}

TEST(BenchmarkAverageRankHistory, IgnoresFactsForNonResolvedMappings) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 300.0F)};
    const auto projection = evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
        {mappedUnavailableEntry("one", "one"), unresolvedEntry("two")}, facts, {});
    EXPECT_TRUE(projection.averageRankHistory.empty());
}

TEST(BenchmarkAverageRankHistory, IsAbsentForIncompleteDefinitionsAndUsesZeroForUnresolvedEntries) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 300.0F),
                                   benchmarkFact("two", 101 * kDay, 300.0F)};
    const auto incomplete = evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Incomplete, {}},
        {resolvedEntry("one", "one"), resolvedEntry("two", "two")}, facts, {});
    EXPECT_TRUE(incomplete.averageRankHistory.empty());
    const auto unresolved = evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
        {resolvedEntry("one", "one"), unresolvedEntry("two")}, facts, {});
    ASSERT_EQ(unresolved.averageRankHistory.size(), 1U);
    EXPECT_DOUBLE_EQ(unresolved.averageRankHistory.back().averageRank, 1.5);
}

TEST(BenchmarkAttainment, AScoreEqualToAThresholdAttainsThatTier) {
    const auto at = [](float score) {
        return scenarioNamed(evaluateBenchmark(benchmark(), {Completeness::Trackable, {}},
                                               {resolvedEntry("entry", "hash")},
                                               {benchmarkFact("hash", kDay, score)}, {}),
                             "entry");
    };
    const auto exact = at(200.0F);
    ASSERT_TRUE(exact.attainedTier);
    EXPECT_EQ(exact.attainedTier->value, "silver");
    ASSERT_TRUE(exact.nextThreshold);
    EXPECT_EQ(exact.nextThreshold->tierId.value, "gold");

    const auto justBelow = at(199.5F);
    ASSERT_TRUE(justBelow.attainedTier);
    EXPECT_EQ(justBelow.attainedTier->value, "bronze");
    ASSERT_TRUE(justBelow.nextThreshold);
    EXPECT_EQ(justBelow.nextThreshold->tierId.value, "silver");
}

TEST(BenchmarkAverageRank, WeighsEveryScenarioEquallyAndCountsAnUnplayedOneAsZero) {
    const auto projection = evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
        {resolvedEntry("one", "one"), unresolvedEntry("two")},
        {benchmarkFact("one", 100 * kDay, 250.0F)}, {});
    EXPECT_DOUBLE_EQ(scenarioNamed(projection, "one").normalizedRank, 2.5);
    EXPECT_DOUBLE_EQ(scenarioNamed(projection, "two").normalizedRank, 0.0);
    ASSERT_TRUE(projection.averageRank);
    EXPECT_DOUBLE_EQ(*projection.averageRank, (2.5 + 0.0) / 2.0);
}

TEST(BenchmarkAverageRank, AScoreAboveTheTopTierCountsAsTheTopTierWithoutCappingThePersonalBest) {
    const auto projection = evaluateBenchmark(benchmark(), {Completeness::Trackable, {}},
        {resolvedEntry("entry", "hash")}, {benchmarkFact("hash", 100 * kDay, 450.0F)}, {});
    const auto &scenario = scenarioNamed(projection, "entry");
    ASSERT_TRUE(scenario.personalBest);
    EXPECT_DOUBLE_EQ(*scenario.personalBest, 450.0);
    ASSERT_TRUE(scenario.recentAverage);
    EXPECT_DOUBLE_EQ(*scenario.recentAverage, 450.0);
    EXPECT_DOUBLE_EQ(scenario.normalizedRank, 3.0);
    ASSERT_TRUE(projection.averageRank);
    EXPECT_DOUBLE_EQ(*projection.averageRank, 3.0);
    ASSERT_EQ(projection.averageRankHistory.size(), 1U);
    EXPECT_DOUBLE_EQ(projection.averageRankHistory.back().averageRank, 3.0);
}

TEST(BenchmarkRecentAverage, UsesExactlyTheLatestFiveRunsAndNeverChangesAttainment) {
    // An early silver PB followed by six weak runs: the recent five sit below bronze.
    std::vector<RunFact> facts{benchmarkFact("hash", 100 * kDay, 250.0F)};
    for (int run = 1; run <= 6; ++run)
        facts.push_back(benchmarkFact("hash", (100 + run) * kDay, static_cast<float>(10 * run)));
    const auto projection = evaluateBenchmark(benchmark(), {Completeness::Trackable, {}},
                                              {resolvedEntry("entry", "hash")}, facts, {});
    const auto &scenario = scenarioNamed(projection, "entry");
    ASSERT_TRUE(scenario.recentAverage);
    EXPECT_DOUBLE_EQ(*scenario.recentAverage, (20.0 + 30.0 + 40.0 + 50.0 + 60.0) / 5.0);
    EXPECT_EQ(scenario.recentSampleCount, 5);
    EXPECT_EQ(scenario.runCount, 7);
    ASSERT_TRUE(scenario.personalBest);
    EXPECT_DOUBLE_EQ(*scenario.personalBest, 250.0);
    ASSERT_TRUE(scenario.attainedTier);
    EXPECT_EQ(scenario.attainedTier->value, "silver");
    EXPECT_DOUBLE_EQ(scenario.normalizedRank, 2.5);
    ASSERT_TRUE(projection.attainedRank);
    EXPECT_EQ(projection.attainedRank->value, "silver");
    ASSERT_TRUE(projection.averageRank);
    EXPECT_DOUBLE_EQ(*projection.averageRank, 2.5);
}

// Category structure decides official attainment but never the average: the same runs read
// under a recategorized definition change the attained rank and leave the average line alone.
TEST(BenchmarkRank, RecategorizingChangesAttainmentButNotTheAverageRank) {
    const auto facts = std::vector{benchmarkFact("one", 100 * kDay, 300.0F),
                                   benchmarkFact("two", 101 * kDay, 100.0F)};
    const auto resolutions = std::vector{resolvedEntry("one", "one"), resolvedEntry("two", "two")};
    const auto separate = evaluateBenchmark(twoScenarioBenchmark(), {Completeness::Trackable, {}},
                                            resolutions, facts, {});

    auto grouped = twoScenarioBenchmark();
    Category interchangeable{GroupId{"either"}, "Either", {}, grouped.uncategorized, {}};
    grouped.uncategorized.clear();
    grouped.categories = {interchangeable};
    const auto together = evaluateBenchmark(grouped, {Completeness::Trackable, {}}, resolutions, facts, {});

    ASSERT_TRUE(separate.attainedRank);
    EXPECT_EQ(separate.attainedRank->value, "bronze");
    ASSERT_TRUE(together.attainedRank);
    EXPECT_EQ(together.attainedRank->value, "gold");
    // Completion still needs every scenario, whatever the grouping.
    ASSERT_TRUE(separate.completedRank);
    ASSERT_TRUE(together.completedRank);
    EXPECT_EQ(separate.completedRank->value, "bronze");
    EXPECT_EQ(together.completedRank->value, "bronze");

    ASSERT_TRUE(separate.averageRank);
    ASSERT_TRUE(together.averageRank);
    EXPECT_DOUBLE_EQ(*separate.averageRank, 2.0);
    EXPECT_DOUBLE_EQ(*together.averageRank, 2.0);
    ASSERT_EQ(separate.averageRankHistory.size(), together.averageRankHistory.size());
    for (std::size_t index = 0; index < separate.averageRankHistory.size(); ++index)
        EXPECT_DOUBLE_EQ(separate.averageRankHistory[index].averageRank,
                         together.averageRankHistory[index].averageRank);
}

TEST(BenchmarkEvaluator, LinkedHashesCombineChronologicalStatisticsAndHistory) {
    auto definition = benchmark();
    definition.uncategorized.front().additionalHashes = {"new"};
    auto resolution = resolvedEntry("entry", "hash");
    resolution.additionalHashes = {"new", "hash", "new"};
    std::vector<RunFact> facts;
    for (int day = 1; day <= 6; ++day)
        facts.push_back(benchmarkFact(day % 2 ? "hash" : "new", day * kDay,
                                      static_cast<float>(day * 50), 10.0F));
    facts.push_back(benchmarkFact("unrelated", 7 * kDay, 999.0F, 100.0F));
    const auto projection = evaluateBenchmark(definition, validateBenchmark(definition),
                                               {resolution}, facts, {});
    const auto &scenario = scenarioNamed(projection, "entry");
    EXPECT_EQ(scenario.runCount, 6);
    ASSERT_TRUE(scenario.personalBest);
    EXPECT_DOUBLE_EQ(*scenario.personalBest, 300.0);
    ASSERT_TRUE(scenario.recentAverage);
    EXPECT_DOUBLE_EQ(*scenario.recentAverage, 200.0);
    EXPECT_EQ(scenario.recentSampleCount, 5);
    EXPECT_DOUBLE_EQ(scenario.playtimeSeconds, 60.0);
    EXPECT_DOUBLE_EQ(projection.totalPlaytimeSeconds, 60.0);
    ASSERT_TRUE(projection.attainedRank);
    EXPECT_EQ(projection.attainedRank->value, "gold");
    ASSERT_EQ(projection.averageRankHistory.size(), 6U);
    for (std::size_t index = 0; index < 6; ++index)
        EXPECT_DOUBLE_EQ(projection.averageRankHistory[index].averageRank, .5 * (index + 1));
}

TEST(BenchmarkEvaluator, LinkedHashesDoNotDoubleCountOverlappingEntryPlaytime) {
    auto definition = twoScenarioBenchmark();
    auto first = resolvedEntry("one", "one");
    first.additionalHashes = {"shared"};
    auto second = resolvedEntry("two", "two");
    second.additionalHashes = {"shared"};
    const auto projection = evaluateBenchmark(definition, {Completeness::Incomplete, {}},
        {first, second}, {benchmarkFact("one", kDay, 100.0F, 10.0F),
                          benchmarkFact("shared", 2 * kDay, 200.0F, 20.0F),
                          benchmarkFact("two", 3 * kDay, 300.0F, 30.0F)}, {});
    EXPECT_DOUBLE_EQ(scenarioNamed(projection, "one").playtimeSeconds, 30.0);
    EXPECT_DOUBLE_EQ(scenarioNamed(projection, "two").playtimeSeconds, 50.0);
    EXPECT_DOUBLE_EQ(projection.totalPlaytimeSeconds, 60.0);
}
