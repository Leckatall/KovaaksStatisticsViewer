#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_PROJECTION_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_PROJECTION_H

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_resolution.h"
#include "benchmarks/benchmark_validation.h"

namespace ksv::domain {
    struct ScenarioProjection {
        ScenarioEntryId entryId;
        std::string name;
        ScenarioMatchState matchState = ScenarioMatchState::Unresolved;
        std::optional<double> personalBest;
        std::optional<double> recentAverage;
        int recentSampleCount = 0;
        int runCount = 0;
        std::optional<TierId> attainedTier;
        std::optional<Threshold> nextThreshold;
        double normalizedRank = 0.0;
        double playtimeSeconds = 0.0;
    };

    struct GroupProjection {
        GroupId id;
        std::string name;
        std::optional<TierId> satisfiedTier;
        std::vector<ScenarioEntryId> scenarios;
        std::vector<GroupProjection> subgroups;
    };

    struct RankBlocker {
        ScenarioEntryId entryId;
        std::optional<GroupId> group;
        double required = 0.0;
        std::optional<double> current;
    };

    struct AverageRankPoint { std::chrono::sys_days day; double averageRank = 0.0; };

    struct BenchmarkProjection {
        BenchmarkId benchmarkId;
        Completeness completeness = Completeness::Incomplete;
        std::vector<Tier> tiers;
        std::vector<ScenarioProjection> scenarios;
        std::vector<ScenarioEntryId> uncategorized;
        std::vector<GroupProjection> categories;
        std::optional<TierId> attainedRank;
        std::optional<TierId> completedRank;
        std::optional<TierId> nextTier;
        std::vector<RankBlocker> nextTierBlockers;
        int scenariosAtNextTier = 0;
        std::optional<double> averageRank;
        std::vector<AverageRankPoint> averageRankHistory;
        double totalPlaytimeSeconds = 0.0;
        std::vector<std::pair<std::chrono::sys_days, double> > rollingPlaytime;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_PROJECTION_H
