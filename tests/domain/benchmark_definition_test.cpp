#include <gtest/gtest.h>

#include <limits>

#include "benchmarks/benchmark_definition.h"

using namespace ksv::domain;

namespace {
    BenchmarkScenario entry(const char *id) { return {.id = {id}}; }

    const TierId bronze{"bronze"};
    const TierId silver{"silver"};
    constexpr double infinity = std::numeric_limits<double>::infinity();
    constexpr double nanScore = std::numeric_limits<double>::quiet_NaN();

    struct LadderCase {
        const char *name;
        std::vector<Threshold> thresholds;
        std::optional<std::vector<double>> expected;
    };

    const LadderCase ladderCases[] = {
        {"tier order", {{silver, 40}, {bronze, 0}}, std::vector<double>{0, 40}},
        {"missing", {{bronze, 0}}, std::nullopt},
        {"duplicate", {{bronze, 0}, {silver, 40}, {silver, 40}}, std::nullopt},
        {"negative", {{bronze, -1}, {silver, 40}}, std::nullopt},
        {"infinite", {{bronze, 0}, {silver, infinity}}, std::nullopt},
        {"NaN", {{bronze, 0}, {silver, nanScore}}, std::nullopt},
        {"equal", {{bronze, 40}, {silver, 40}}, std::nullopt},
        {"decreasing", {{bronze, 50}, {silver, 40}}, std::nullopt},
    };

    BenchmarkDefinition twoTiers() { return {.tiers = {{.id = bronze}, {.id = silver}}}; }
}

TEST(BenchmarkDefinition, EntriesPointIntoTheDefinitionInCanonicalOrder) {
    const BenchmarkDefinition definition{
        .uncategorized = {entry("u1"), entry("u2")},
        .categories = {
            {.scenarios = {entry("d1"), entry("d2")}},
            {.subcategories = {{.scenarios = {entry("a1"), entry("a2")}}, {.scenarios = {entry("b1")}}}},
            {.scenarios = {entry("mixed-direct")},
             .subcategories = {{.scenarios = {entry("mixed-first")}}, {.scenarios = {entry("mixed-second")}}}}
        }
    };
    const auto &categories = definition.categories;
    const std::vector<const BenchmarkScenario *> expected{
        &definition.uncategorized[0], &definition.uncategorized[1],
        &categories[0].scenarios[0], &categories[0].scenarios[1],
        &categories[1].subcategories[0].scenarios[0], &categories[1].subcategories[0].scenarios[1],
        &categories[1].subcategories[1].scenarios[0],
        &categories[2].scenarios[0], &categories[2].subcategories[0].scenarios[0],
        &categories[2].subcategories[1].scenarios[0]
    };

    EXPECT_EQ(definition.entries(), expected);
    EXPECT_TRUE(BenchmarkDefinition{}.entries().empty());
}

TEST(BenchmarkDefinition, ScenarioHashesKeepFirstOccurrencesAcrossEntries) {
    const BenchmarkDefinition definition{
        .uncategorized = {{.hashes = {"z", "a"}}, {}},
        .categories = {{.scenarios = {{.hashes = {"a", "m"}}},
                        .subcategories = {{.scenarios = {{.hashes = {"z", "b"}}}}}}}
    };

    EXPECT_EQ(definition.scenarioHashes(), (std::vector<std::string>{"z", "a", "m", "b"}));
}

TEST(BenchmarkDefinition, ThresholdForAdmitsExactlyOneFiniteNonNegativeScore) {
    const BenchmarkScenario admitted{.thresholds = {{bronze, 0}, {silver, 25}}};
    EXPECT_EQ(admitted.thresholdFor(bronze), 0.0);
    EXPECT_EQ(admitted.thresholdFor(silver), 25.0);
    EXPECT_EQ(admitted.thresholdFor(TierId{"absent"}), std::nullopt);

    for (const double rejected: {-1.0, infinity, nanScore}) {
        const BenchmarkScenario candidate{.thresholds = {{bronze, rejected}}};
        EXPECT_EQ(candidate.thresholdFor(bronze), std::nullopt);
    }
    const BenchmarkScenario duplicate{.thresholds = {{bronze, 0}, {bronze, 12}}};
    EXPECT_EQ(duplicate.thresholdFor(bronze), std::nullopt);
}

TEST(BenchmarkDefinition, BrokenHigherTierLeavesTheGoodThresholdAdmitted) {
    const auto definition = twoTiers();
    const BenchmarkScenario candidate{.thresholds = {{bronze, 5}, {silver, infinity}}};

    EXPECT_EQ(candidate.thresholdFor(bronze), 5.0);
    EXPECT_EQ(definition.ladderFor(candidate), std::nullopt);
}

TEST(BenchmarkDefinition, LadderRequiresEveryTierAndStrictlyIncreasingScoresInTierOrder) {
    const auto definition = twoTiers();
    for (const auto &test: ladderCases) {
        SCOPED_TRACE(test.name);
        EXPECT_EQ(definition.ladderFor(BenchmarkScenario{.thresholds = test.thresholds}), test.expected);
    }
    EXPECT_EQ(BenchmarkDefinition{}.ladderFor(BenchmarkScenario{.thresholds = ladderCases[0].thresholds}), std::nullopt);
}
