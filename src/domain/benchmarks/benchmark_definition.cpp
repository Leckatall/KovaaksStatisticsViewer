#include "benchmarks/benchmark_definition.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace ksv::domain {
    std::optional<double> BenchmarkScenario::thresholdFor(const TierId &tier) const {
        std::optional<double> found;
        for (const auto &threshold: thresholds) {
            if (threshold.tierId != tier) continue;
            if (found) return std::nullopt;
            found = threshold.score;
        }
        if (found && (!std::isfinite(*found) || *found < 0.0)) return std::nullopt;
        return found;
    }

    namespace {
        // One traversal for both constnesses: List is std::vector<BenchmarkScenario> with matching const.
        template<class List, class Definition>
        std::vector<List *> scenarioListsOf(Definition &definition) {
            std::vector<List *> lists{&definition.uncategorized};
            for (auto &category: definition.categories) {
                lists.push_back(&category.scenarios);
                for (auto &subcategory: category.subcategories) lists.push_back(&subcategory.scenarios);
            }
            return lists;
        }

        template<class Scenario, class Lists>
        Scenario *findIn(const Lists &lists, const BenchmarkScenarioId &id) {
            for (auto *list: lists) {
                const auto found = std::ranges::find(*list, id, &BenchmarkScenario::id);
                if (found != list->end()) return &*found;
            }
            return nullptr;
        }

        template<class Group, class Definition>
        std::vector<Group *> groupsOf(Definition &definition) {
            std::vector<Group *> groups;
            for (auto &category: definition.categories) {
                groups.push_back(&category);
                for (auto &subcategory: category.subcategories) groups.push_back(&subcategory);
            }
            return groups;
        }

        template<class Group>
        Group *findGroup(const std::vector<Group *> &groups, const GroupId &id) {
            const auto found = std::ranges::find(groups, id, &BenchmarkGroup::id);
            return found == groups.end() ? nullptr : *found;
        }
    }

    std::vector<BenchmarkGroup *> BenchmarkDefinition::groups() { return groupsOf<BenchmarkGroup>(*this); }

    std::vector<const BenchmarkGroup *> BenchmarkDefinition::groups() const {
        return groupsOf<const BenchmarkGroup>(*this);
    }

    BenchmarkGroup *BenchmarkDefinition::group(const GroupId &id) { return findGroup(groups(), id); }

    const BenchmarkGroup *BenchmarkDefinition::group(const GroupId &id) const { return findGroup(groups(), id); }

    std::vector<std::vector<BenchmarkScenario> *> BenchmarkDefinition::scenarioLists() {
        return scenarioListsOf<std::vector<BenchmarkScenario>>(*this);
    }

    std::vector<const std::vector<BenchmarkScenario> *> BenchmarkDefinition::scenarioLists() const {
        return scenarioListsOf<const std::vector<BenchmarkScenario>>(*this);
    }

    std::vector<const BenchmarkScenario *> BenchmarkDefinition::entries() const {
        std::vector<const BenchmarkScenario *> ordered;
        for (const auto *list: scenarioLists())
            for (const auto &entry: *list) ordered.push_back(&entry);
        return ordered;
    }

    BenchmarkScenario *BenchmarkDefinition::entry(const BenchmarkScenarioId &id) {
        return findIn<BenchmarkScenario>(scenarioLists(), id);
    }

    const BenchmarkScenario *BenchmarkDefinition::entry(const BenchmarkScenarioId &id) const {
        return findIn<const BenchmarkScenario>(scenarioLists(), id);
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

    std::optional<std::vector<double>> BenchmarkDefinition::ladderFor(const BenchmarkScenario &entry) const {
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
