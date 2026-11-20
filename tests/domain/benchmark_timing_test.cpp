#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "benchmark_builders.h"
#include "benchmarks/benchmark.h"
#include "run_builders.h"
#include "user_profile.h"

using namespace ksv::domain;
using ksv::tests_support::benchmarkEntry;
using ksv::tests_support::benchmarkTier;
using ksv::tests_support::makeRun;

namespace {
    constexpr int kScenarios = 30;
    constexpr int kRunsPerScenario = 2000;
    constexpr long long kDay = 86'400'000LL;
    constexpr double kCeilingMs = 2000.0;

    std::string hashOf(const int index) { return "hash-" + std::to_string(index); }
}

TEST(BenchmarkTiming, AFullProjectionOverALargeProfileStaysWellUnderASecond) {
    UserProfile profile;
    std::vector<ScenarioId> scenarios;
    BenchmarkDefinition definition;
    definition.id = BenchmarkId{"timing"};
    definition.name = "Timing";
    definition.tiers = {benchmarkTier("bronze"), benchmarkTier("silver"), benchmarkTier("gold")};
    std::vector<ScenarioResolution> resolutions;

    for (int scenario = 0; scenario < kScenarios; ++scenario) {
        const auto hash = hashOf(scenario);
        scenarios.push_back({"Scenario " + hash, hash});
        const auto entryId = "e" + std::to_string(scenario);
        definition.uncategorized.push_back(benchmarkEntry(
            entryId, "Scenario " + hash, hash,
            {{"bronze", 100.0}, {"silver", 200.0}, {"gold", 300.0}}));
        resolutions.push_back(ksv::tests_support::resolvedEntry(entryId));
        for (int run = 0; run < kRunsPerScenario; ++run) {
            profile.addRun(makeRun(hash, (1000 + run) * kDay + scenario,
                                   static_cast<float>(run) * 0.2F, 0, 0, 60.0F));
        }
    }

    const auto start = std::chrono::steady_clock::now();
    const auto facts = profile.getRunFacts(scenarios);
    const auto evaluated = Benchmark(definition, {Completeness::Trackable, {}}, resolutions, facts);
    const auto rolling = evaluated.rollingPlaytime(3);
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    std::cout << "[ TIMING   ] " << kScenarios << " scenarios x " << kRunsPerScenario
              << " runs = " << facts.size() << " facts, "
              << evaluated.averageRankHistory().size() << " history points, "
              << elapsed << " ms" << std::endl;
    EXPECT_EQ(facts.size(), static_cast<std::size_t>(kScenarios) * kRunsPerScenario);
    EXPECT_FALSE(evaluated.averageRankHistory().empty());
    EXPECT_FALSE(rolling.empty());
    EXPECT_LT(elapsed, kCeilingMs);
}
