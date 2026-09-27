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
        // Matches the blank-name precondition the benchmarks service enforces on save, so a
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
                if (++perTier[threshold.tierId.value] == 2)
                    issues.push_back({BenchmarkIssueCode::DuplicateThreshold, entry.id, threshold.tierId});
                if (!std::isfinite(threshold.score))
                    issues.push_back({BenchmarkIssueCode::NonFiniteThreshold, entry.id, threshold.tierId});
                else if (threshold.score < 0.0)
                    issues.push_back({BenchmarkIssueCode::NegativeThreshold, entry.id, threshold.tierId});
            }

            // Each tier whose score fails to exceed the last finite score before it (in current
            // ladder order) is reported, so the offending cell is addressable.
            double previous = -std::numeric_limits<double>::infinity();
            for (const auto &tier: tiers) {
                const auto found = std::ranges::find_if(
                    entry.thresholds, [&](const auto &t) { return t.tierId == tier.id; });
                if (found == entry.thresholds.end()) {
                    issues.push_back({BenchmarkIssueCode::MissingThreshold, entry.id, tier.id});
                    continue;
                }
                if (std::isfinite(found->score)) {
                    if (found->score <= previous)
                        issues.push_back({BenchmarkIssueCode::NonIncreasingThreshold, entry.id, tier.id});
                    previous = found->score;
                }
            }
        }
    }

    CompletenessResult validateBenchmark(const Benchmark &def) {
        CompletenessResult result;
        auto &issues = result.issues;

        if (isBlank(def.name)) issues.push_back({BenchmarkIssueCode::MissingName, def.id});
        if (def.tiers.empty()) issues.push_back({BenchmarkIssueCode::NoTiers, def.id});

        std::unordered_set<std::string> tierNames;
        for (const auto &tier: def.tiers) {
            if (isBlank(tier.name)) issues.push_back({BenchmarkIssueCode::MissingTierName, tier.id});
            else if (!tierNames.insert(tier.name).second)
                issues.push_back({BenchmarkIssueCode::DuplicateTierName, tier.id});
        }

        for (const auto &category: def.categories) {
            if (isBlank(category.name)) issues.push_back({BenchmarkIssueCode::MissingGroupName, category.id});
            for (const auto &sub: category.subcategories)
                if (isBlank(sub.name)) issues.push_back({BenchmarkIssueCode::MissingGroupName, sub.id});
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
            // Unnamed rows are reported once each as missing names, never as duplicates of one another.
            if (isBlank(entry->name))
                issues.push_back({BenchmarkIssueCode::MissingScenarioName, entry->id});
            else if (!names.insert(entry->name).second)
                issues.push_back({BenchmarkIssueCode::DuplicateScenarioMembership, entry->id});
            if (entry->hash && !hashes.insert(*entry->hash).second)
                issues.push_back({BenchmarkIssueCode::DuplicateResolvedHash, entry->id});
            if (!def.tiers.empty()) validateEntryThresholds(*entry, def.tiers, issues);
        }

        result.completeness = issues.empty() ? Completeness::Trackable : Completeness::Incomplete;
        return result;
    }
}
