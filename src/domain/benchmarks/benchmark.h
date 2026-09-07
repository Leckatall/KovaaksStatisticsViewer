#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_H

#include <compare>
#include <optional>
#include <string>
#include <vector>

#include "benchmark_ids.h"

namespace ksv::domain {
    struct Tier {
        TierId id;
        std::string name;
        BenchmarkColor color;
        auto operator<=>(const Tier &) const = default;
    };

    struct Threshold {
        TierId tierId;
        double score = 0.0;
        auto operator<=>(const Threshold &) const = default;
    };

    struct ScenarioEntry {
        ScenarioEntryId id;
        std::string name;                  // retained display label; never replaced by the hash
        std::optional<std::string> hash;   // persisted scenario mapping when resolved
        std::vector<Threshold> thresholds; // one per tier when complete; decoded as-is for validation
        auto operator<=>(const ScenarioEntry &) const = default;
    };

    struct Subcategory {
        GroupId id;
        std::string name;
        BenchmarkColor color;
        std::vector<ScenarioEntry> scenarios;
        auto operator<=>(const Subcategory &) const = default;
    };

    // A complete category holds EITHER direct scenarios OR subcategories. Both vectors exist so a
    // decoded or mid-edit definition can represent the incomplete "mixed" and "empty" states that
    // validation reports rather than being unrepresentable.
    struct Category {
        GroupId id;
        std::string name;
        BenchmarkColor color;
        std::vector<ScenarioEntry> scenarios;
        std::vector<Subcategory> subcategories;
        auto operator<=>(const Category &) const = default;
    };

    struct Benchmark {
        BenchmarkId id;
        std::string name;                       // may be empty while incomplete
        std::vector<Tier> tiers;                // ordered ladder
        std::vector<ScenarioEntry> uncategorized;
        std::vector<Category> categories;       // ordered user-created categories
        auto operator<=>(const Benchmark &) const = default;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_H
