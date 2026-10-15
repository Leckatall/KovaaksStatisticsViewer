#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_RESOLUTION_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_RESOLUTION_H

#include <chrono>
#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "benchmarks/benchmark_ids.h"

namespace ksv::domain {
    enum class ScenarioMatchState { Resolved, MappedUnavailable, Unresolved, Ambiguous, AutoMappable };

    // Automatic name matching compares these keys, so a benchmark entry matches a played scenario
    // regardless of case and surrounding whitespace.
    [[nodiscard]] std::string scenarioNameKey(std::string_view name);

    struct ScenarioCandidate {
        std::string hash;
        int runCount = 0;
        std::optional<std::chrono::sys_seconds> lastPlayed;
        // The played scenario's exact name, which an automatic link adopts.
        std::string name;
        auto operator<=>(const ScenarioCandidate &) const = default;
    };

    struct ScenarioResolution {
        ScenarioEntryId entryId;
        ScenarioMatchState state = ScenarioMatchState::Unresolved;
        std::optional<std::string> hash;
        std::vector<ScenarioCandidate> candidates;
        auto operator<=>(const ScenarioResolution &) const = default;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_RESOLUTION_H
