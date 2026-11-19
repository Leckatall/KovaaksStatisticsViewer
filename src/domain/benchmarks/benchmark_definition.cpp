#include "benchmarks/benchmark_definition.h"

#include <cmath>
#include <unordered_set>

namespace ksv::domain {
    std::optional<double> ScenarioEntry::thresholdFor(const TierId &tier) const {
        std::optional<double> found;
        for (const auto &threshold: thresholds) {
            if (threshold.tierId != tier) continue;
            if (found) return std::nullopt;
            found = threshold.score;
        }
        if (found && (!std::isfinite(*found) || *found < 0.0)) return std::nullopt;
        return found;
    }

    std::vector<const ScenarioEntry *> BenchmarkDefinition::entries() const {
        std::vector<const ScenarioEntry *> ordered;
        const auto append = [&](const std::vector<ScenarioEntry> &scenarios) {
            for (const auto &entry: scenarios) ordered.push_back(&entry);
        };
        append(uncategorized);
        for (const auto &category: categories) {
            append(category.scenarios);
            for (const auto &subcategory: category.subcategories) append(subcategory.scenarios);
        }
        return ordered;
    }

    std::vector<std::string> BenchmarkDefinition::scenarioHashes() const {
        std::vector<std::string> hashes;
        std::unordered_set<std::string> seen;
        for (const auto *entry: entries()) {
            for (const auto &hash: entry->hashes) {
                if (seen.insert(hash).second) hashes.push_back(hash);
            }
        }
        return hashes;
    }

    std::optional<std::vector<double>> BenchmarkDefinition::ladderFor(const ScenarioEntry &entry) const {
        if (tiers.empty()) return std::nullopt;
        std::vector<double> ladder;
        ladder.reserve(tiers.size());
        for (const auto &tier: tiers) {
            const auto score = entry.thresholdFor(tier.id);
            if (!score || (!ladder.empty() && *score <= ladder.back())) return std::nullopt;
            ladder.push_back(*score);
        }
        return ladder;
    }
}
