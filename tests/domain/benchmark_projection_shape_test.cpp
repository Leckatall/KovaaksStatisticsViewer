#include <gtest/gtest.h>

#include <type_traits>

#include "benchmarks/benchmark_projection.h"
#include "benchmarks/benchmark_resolution.h"

using namespace ksv::domain;

TEST(BenchmarkProjectionShape, DefaultsToNothingKnown) {
    const BenchmarkProjection projection;
    EXPECT_EQ(projection.completeness, Completeness::Incomplete);
    EXPECT_TRUE(projection.scenarios.empty());
    EXPECT_TRUE(projection.uncategorized.empty());
    EXPECT_TRUE(projection.categories.empty());
    EXPECT_FALSE(projection.attainedRank);
    EXPECT_FALSE(projection.completedRank);
    EXPECT_FALSE(projection.nextTier);
    EXPECT_TRUE(projection.nextTierBlockers.empty());
    EXPECT_EQ(projection.scenariosAtNextTier, 0);
    EXPECT_FALSE(projection.averageRank);
    EXPECT_TRUE(projection.averageRankHistory.empty());
    EXPECT_DOUBLE_EQ(projection.totalPlaytimeSeconds, 0.0);
    EXPECT_TRUE(projection.rollingPlaytime.empty());
}

TEST(BenchmarkProjectionShape, DefaultsAnUnknownEntryToUnresolved) {
    const ScenarioResolution resolution;
    EXPECT_EQ(resolution.state, ScenarioMatchState::Unresolved);
    EXPECT_FALSE(resolution.hash);
    EXPECT_TRUE(resolution.candidates.empty());
}

TEST(BenchmarkProjectionShape, IsCopyableAndMovable) {
    EXPECT_TRUE(std::is_copy_constructible_v<BenchmarkProjection>);
    EXPECT_TRUE(std::is_move_constructible_v<BenchmarkProjection>);
    EXPECT_TRUE(std::is_copy_assignable_v<BenchmarkProjection>);
}
