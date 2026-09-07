#include "benchmark_validation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

namespace ksv::domain {
    namespace {
        // Matches the blank-name precondition BenchmarkLibraryService::saveDraft enforces, so a
        // whitespace-only name cannot validate as Trackable and then be refused by the save.
        bool isBlank(const std::string &text) {
            return std::ranges::all_of(text, [](unsigned char ch) { return std::isspace(ch) != 0; });
        }

        void collectEntries(const Benchmark &def, std::vector<const ScenarioEntry *> &out) {
            for (const auto &e: def.uncategorized) out.push_back(&e);
            for (const auto &category: def.categories) {
                for (const auto &e: category.scenarios) out.push_back(&e);
                for (const auto &sub: category.subcategories)
                    for (const auto &e: sub.scenarios) out.push_back(&e);
            }
        }

        void validateEntryThresholds(const ScenarioEntry &entry, const std::vector<Tier> &tiers,
                                     std::vector<BenchmarkIssue> &issues) {
            std::unordered_map<std::string, int> perTier;
            for (const auto &threshold: entry.thresholds) {
                perTier[threshold.tierId.value]++;
                if (!std::isfinite(threshold.score))
                    issues.push_back({BenchmarkIssueCode::NonFiniteThreshold, entry.id});
                else if (threshold.score < 0.0)
                    issues.push_back({BenchmarkIssueCode::NegativeThreshold, entry.id});
            }
            for (const auto &[tierId, count]: perTier)
                if (count > 1) issues.push_back({BenchmarkIssueCode::DuplicateThreshold, entry.id});

            double previous = -std::numeric_limits<double>::infinity();
            bool increasing = true;
            for (const auto &tier: tiers) {
                const auto found = std::ranges::find_if(
                    entry.thresholds, [&](const auto &t) { return t.tierId == tier.id; });
                if (found == entry.thresholds.end()) {
                    issues.push_back({BenchmarkIssueCode::MissingThreshold, entry.id});
                    continue;
                }
                if (std::isfinite(found->score)) {
                    if (found->score <= previous) increasing = false;
                    previous = found->score;
                }
            }
            if (!increasing) issues.push_back({BenchmarkIssueCode::NonIncreasingThreshold, entry.id});
        }
    }

    CompletenessResult validateBenchmark(const Benchmark &def) {
        CompletenessResult result;
        auto &issues = result.issues;

        if (isBlank(def.name)) issues.push_back({BenchmarkIssueCode::MissingName, def.id});
        if (def.tiers.empty()) issues.push_back({BenchmarkIssueCode::NoTiers, def.id});

        std::unordered_set<std::string> tierNames;
        for (const auto &tier: def.tiers)
            if (!tierNames.insert(tier.name).second)
                issues.push_back({BenchmarkIssueCode::DuplicateTierName, tier.id});

        for (const auto &category: def.categories) {
            const bool hasScenarios = !category.scenarios.empty();
            const bool hasSubcategories = !category.subcategories.empty();
            if (hasScenarios && hasSubcategories)
                issues.push_back({BenchmarkIssueCode::MixedCategoryContent, category.id});
            if (!hasScenarios && !hasSubcategories)
                issues.push_back({BenchmarkIssueCode::EmptyCategory, category.id});
            for (const auto &sub: category.subcategories)
                if (sub.scenarios.empty())
                    issues.push_back({BenchmarkIssueCode::EmptySubcategory, sub.id});
        }

        std::vector<const ScenarioEntry *> entries;
        collectEntries(def, entries);
        if (entries.empty()) issues.push_back({BenchmarkIssueCode::NoScenarios, def.id});

        std::unordered_set<std::string> names;
        std::unordered_set<std::string> hashes;
        for (const auto *entry: entries) {
            if (!names.insert(entry->name).second)
                issues.push_back({BenchmarkIssueCode::DuplicateScenarioMembership, entry->id});
            if (entry->hash && !hashes.insert(*entry->hash).second)
                issues.push_back({BenchmarkIssueCode::DuplicateResolvedHash, entry->id});
            if (!def.tiers.empty()) validateEntryThresholds(*entry, def.tiers, issues);
        }

        result.completeness = issues.empty() ? Completeness::Trackable : Completeness::Incomplete;
        return result;
    }
}
