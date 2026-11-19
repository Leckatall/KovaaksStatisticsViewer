#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_DEFINITION_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_DEFINITION_H

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
        std::vector<std::string> hashes;   // unique, non-empty, in user order; writers enforce this
        std::vector<Threshold> thresholds; // one per tier when complete; decoded as-is for validation
        auto operator<=>(const ScenarioEntry &) const = default;

        /** @return The tier's score when exactly one threshold names it and it is finite and non-negative. */
        [[nodiscard]] std::optional<double> thresholdFor(const TierId &tier) const;
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

    struct BenchmarkDefinition {
        BenchmarkId id;
        std::string name;                       // may be empty while incomplete
        std::vector<Tier> tiers;                // ordered ladder
        std::vector<ScenarioEntry> uncategorized;
        std::vector<Category> categories;       // ordered user-created categories
        auto operator<=>(const BenchmarkDefinition &) const = default;

        /**
         * Uncategorized; then per category its direct scenarios, then each subcategory's.
         * Pointers remain valid while the definition is alive and unmodified.
         */
        [[nodiscard]] std::vector<const ScenarioEntry *> entries() const;
        /** Unique, in first-occurrence order over entries(). */
        [[nodiscard]] std::vector<std::string> scenarioHashes() const;
        /** Every tier admitted and strictly increasing, in tier order; otherwise nullopt. */
        [[nodiscard]] std::optional<std::vector<double>> ladderFor(const ScenarioEntry &entry) const;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_DEFINITION_H
