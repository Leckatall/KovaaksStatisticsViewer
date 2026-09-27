#include "benchmark_editor.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace ksv::domain {
    namespace {
        // Visits every scenario list in the hierarchy in editing order: uncategorized, then each
        // category's direct scenarios and each subcategory's scenarios. `fn` returns true to stop
        // early (found), false to keep visiting; the return is whether any visit stopped.
        template <class BenchmarkT, class Fn>
        bool forEachScenarioList(BenchmarkT &benchmark, Fn &&fn) {
            if (fn(benchmark.uncategorized)) return true;
            for (auto &category: benchmark.categories) {
                if (fn(category.scenarios)) return true;
                for (auto &sub: category.subcategories)
                    if (fn(sub.scenarios)) return true;
            }
            return false;
        }

        Tier *findTier(Benchmark &benchmark, const TierId &id) {
            const auto it = std::ranges::find_if(benchmark.tiers,
                                                 [&](const Tier &tier) { return tier.id == id; });
            return it == benchmark.tiers.end() ? nullptr : &*it;
        }

        ScenarioEntry *findEntry(Benchmark &benchmark, const ScenarioEntryId &id) {
            ScenarioEntry *found = nullptr;
            forEachScenarioList(benchmark, [&](std::vector<ScenarioEntry> &entries) {
                const auto it = std::ranges::find_if(entries,
                                                     [&](const ScenarioEntry &entry) { return entry.id == id; });
                if (it == entries.end()) return false;
                found = &*it;
                return true;
            });
            return found;
        }

        Category *findCategory(Benchmark &benchmark, const GroupId &id) {
            const auto it = std::ranges::find_if(benchmark.categories,
                                                 [&](const Category &category) { return category.id == id; });
            return it == benchmark.categories.end() ? nullptr : &*it;
        }

        Subcategory *findSubcategory(Benchmark &benchmark, const GroupId &id) {
            for (auto &category: benchmark.categories)
                for (auto &sub: category.subcategories)
                    if (sub.id == id) return &sub;
            return nullptr;
        }

        std::optional<ScenarioEntry> detachEntry(Benchmark &benchmark, const ScenarioEntryId &id) {
            std::optional<ScenarioEntry> detached;
            forEachScenarioList(benchmark, [&](std::vector<ScenarioEntry> &entries) {
                const auto it = std::ranges::find_if(entries,
                                                     [&](const ScenarioEntry &entry) { return entry.id == id; });
                if (it == entries.end()) return false;
                detached = *it;
                entries.erase(it);
                return true;
            });
            return detached;
        }

        bool nameExists(const Benchmark &benchmark, const std::string &name) {
            return forEachScenarioList(benchmark, [&](const std::vector<ScenarioEntry> &entries) {
                return std::ranges::any_of(entries,
                                           [&](const ScenarioEntry &entry) { return entry.name == name; });
            });
        }

        // Moves `*it` to `position` within `items`; false when `position` is out of range.
        template <class T>
        bool moveToPosition(std::vector<T> &items, typename std::vector<T>::iterator it, std::size_t position) {
            if (position >= items.size()) return false;
            T item = std::move(*it);
            items.erase(it);
            items.insert(items.begin() + static_cast<std::ptrdiff_t>(position), std::move(item));
            return true;
        }

        // Checked before anything is detached, so a rejected move leaves every entry in place.
        std::optional<BenchmarkEditError> groupTargetError(Benchmark &benchmark, const EditorGroupTarget &to) {
            const auto *targetGroup = std::get_if<GroupId>(&to);
            if (!targetGroup) return std::nullopt;
            const auto *category = findCategory(benchmark, *targetGroup);
            if (!category && !findSubcategory(benchmark, *targetGroup)) return BenchmarkEditError::UnknownGroup;
            if (category && !category->subcategories.empty()) return BenchmarkEditError::MixedContent;
            return std::nullopt;
        }

        // `to` must already have passed groupTargetError().
        std::vector<ScenarioEntry> &groupScenarios(Benchmark &benchmark, const EditorGroupTarget &to) {
            const auto *targetGroup = std::get_if<GroupId>(&to);
            if (!targetGroup) return benchmark.uncategorized;
            if (auto *category = findCategory(benchmark, *targetGroup)) return category->scenarios;
            return findSubcategory(benchmark, *targetGroup)->scenarios;
        }

        // KSV-assigned defaults for new tiers and groups; users may edit any of them.
        BenchmarkColor paletteColor(std::size_t index) {
            static const BenchmarkColor palette[] = {
                {205, 127, 50, 255},  {192, 192, 192, 255}, {255, 215, 0, 255}, {255, 140, 0, 255},
                {33, 150, 243, 255},  {76, 175, 80, 255},   {156, 39, 176, 255}, {233, 30, 99, 255},
            };
            return palette[index % std::size(palette)];
        }
    }

    BenchmarkEditResult BenchmarkEditor::renameBenchmark(const std::string &name) {
        m_target.name = name;
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::addTier(const std::string &name) {
        const auto id = TierId{newId()};
        m_target.tiers.push_back(Tier{id, name, paletteColor(m_target.tiers.size())});
        return {std::nullopt, id};
    }

    BenchmarkEditResult BenchmarkEditor::renameTier(const TierId &id, const std::string &name) {
        auto *tier = findTier(m_target, id);
        if (!tier) return {BenchmarkEditError::UnknownTier};
        tier->name = name;
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::setTierColor(const TierId &id, BenchmarkColor color) {
        auto *tier = findTier(m_target, id);
        if (!tier) return {BenchmarkEditError::UnknownTier};
        tier->color = color;
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::reorderTier(const TierId &id, std::size_t position) {
        auto &tiers = m_target.tiers;
        const auto it = std::ranges::find_if(tiers, [&](const Tier &tier) { return tier.id == id; });
        if (it == tiers.end()) return {BenchmarkEditError::UnknownTier};
        if (!moveToPosition(tiers, it, position)) return {BenchmarkEditError::PositionOutOfRange};
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::removeTier(const TierId &id) {
        auto &benchmark = m_target;
        const auto tierIt = std::ranges::find_if(benchmark.tiers,
                                                 [&](const Tier &tier) { return tier.id == id; });
        if (tierIt == benchmark.tiers.end()) return {BenchmarkEditError::UnknownTier};
        benchmark.tiers.erase(tierIt);
        forEachScenarioList(benchmark, [&](std::vector<ScenarioEntry> &entries) {
            for (auto &entry: entries)
                std::erase_if(entry.thresholds,
                              [&](const Threshold &threshold) { return threshold.tierId == id; });
            return false;
        });
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::addUnplayedScenario(const std::string &name) {
        if (nameExists(m_target, name)) return {BenchmarkEditError::DuplicateScenarioName};
        const auto id = ScenarioEntryId{newId()};
        m_target.uncategorized.push_back(ScenarioEntry{id, name, std::nullopt, {}});
        return {std::nullopt, std::nullopt, std::nullopt, id};
    }

    BenchmarkEditResult BenchmarkEditor::addKnownScenario(const std::string &name, const std::string &hash) {
        if (nameExists(m_target, name)) return {BenchmarkEditError::DuplicateScenarioName};
        const auto id = ScenarioEntryId{newId()};
        m_target.uncategorized.push_back(ScenarioEntry{id, name, hash, {}});
        return {std::nullopt, std::nullopt, std::nullopt, id};
    }

    BenchmarkEditResult BenchmarkEditor::renameScenario(const ScenarioEntryId &id, const std::string &name) {
        auto *entry = findEntry(m_target, id);
        if (!entry) return {BenchmarkEditError::UnknownEntry};
        entry->name = name;
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::setScenarioHash(const ScenarioEntryId &id,
                                                         const std::optional<std::string> &hash) {
        auto *entry = findEntry(m_target, id);
        if (!entry) return {BenchmarkEditError::UnknownEntry};
        entry->hash = hash;
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::removeScenario(const ScenarioEntryId &id) {
        if (!detachEntry(m_target, id)) return {BenchmarkEditError::UnknownEntry};
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::setThreshold(const ScenarioEntryId &entryId,
                                                      const TierId &tierId, double score) {
        auto &benchmark = m_target;
        if (!findTier(benchmark, tierId)) return {BenchmarkEditError::UnknownTier};
        auto *entry = findEntry(benchmark, entryId);
        if (!entry) return {BenchmarkEditError::UnknownEntry};
        const auto existing = std::ranges::find_if(entry->thresholds,
                                                   [&](const Threshold &threshold) {
                                                       return threshold.tierId == tierId;
                                                   });
        if (existing != entry->thresholds.end()) existing->score = score;
        else entry->thresholds.push_back(Threshold{tierId, score});
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::clearThreshold(const ScenarioEntryId &entryId, const TierId &tierId) {
        if (!findTier(m_target, tierId)) return {BenchmarkEditError::UnknownTier};
        auto *entry = findEntry(m_target, entryId);
        if (!entry) return {BenchmarkEditError::UnknownEntry};
        std::erase_if(entry->thresholds,
                      [&](const Threshold &threshold) { return threshold.tierId == tierId; });
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::addCategory(const std::string &name) {
        const auto id = GroupId{newId()};
        m_target.categories.push_back(Category{id, name, paletteColor(m_target.categories.size()), {}, {}});
        return {std::nullopt, std::nullopt, id};
    }

    BenchmarkEditResult BenchmarkEditor::renameGroup(const GroupId &id, const std::string &name) {
        auto &benchmark = m_target;
        if (auto *category = findCategory(benchmark, id)) category->name = name;
        else if (auto *sub = findSubcategory(benchmark, id)) sub->name = name;
        else return {BenchmarkEditError::UnknownGroup};
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::setGroupColor(const GroupId &id, BenchmarkColor color) {
        auto &benchmark = m_target;
        if (auto *category = findCategory(benchmark, id)) category->color = color;
        else if (auto *sub = findSubcategory(benchmark, id)) sub->color = color;
        else return {BenchmarkEditError::UnknownGroup};
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::reorderCategory(const GroupId &id, std::size_t position) {
        auto &categories = m_target.categories;
        const auto it = std::ranges::find_if(categories,
                                             [&](const Category &category) { return category.id == id; });
        if (it == categories.end()) return {BenchmarkEditError::UnknownGroup};
        if (!moveToPosition(categories, it, position)) return {BenchmarkEditError::PositionOutOfRange};
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::removeCategory(const GroupId &id) {
        auto &benchmark = m_target;
        const auto it = std::ranges::find_if(benchmark.categories,
                                             [&](const Category &category) { return category.id == id; });
        if (it != benchmark.categories.end()) {
            for (auto &entry: it->scenarios) benchmark.uncategorized.push_back(std::move(entry));
            for (auto &sub: it->subcategories)
                for (auto &entry: sub.scenarios) benchmark.uncategorized.push_back(std::move(entry));
            benchmark.categories.erase(it);
            return {};
        }
        // A subcategory's scenarios go back to Uncategorized: the parent may still hold other
        // subcategories, so re-homing them as direct scenarios could create a mixed shape.
        for (auto &category: benchmark.categories) {
            const auto subIt = std::ranges::find_if(category.subcategories,
                                                    [&](const Subcategory &sub) { return sub.id == id; });
            if (subIt == category.subcategories.end()) continue;
            for (auto &entry: subIt->scenarios) benchmark.uncategorized.push_back(std::move(entry));
            category.subcategories.erase(subIt);
            return {};
        }
        return {BenchmarkEditError::UnknownGroup};
    }

    BenchmarkEditResult BenchmarkEditor::addSubcategory(const GroupId &categoryId, const std::string &name) {
        auto *category = findCategory(m_target, categoryId);
        if (!category) return {BenchmarkEditError::UnknownGroup};
        if (!category->scenarios.empty()) return {BenchmarkEditError::MixedContent};
        return addSubcategory(categoryId, name, RelocateDirectToUncategorized{});
    }

    BenchmarkEditResult BenchmarkEditor::addSubcategory(const GroupId &categoryId, const std::string &name,
                                                        const DirectScenarioRelocation &relocation) {
        auto &benchmark = m_target;
        auto *category = findCategory(benchmark, categoryId);
        if (!category) return {BenchmarkEditError::UnknownGroup};
        const auto id = GroupId{newId()};
        std::vector<ScenarioEntry> direct = std::move(category->scenarios);
        category->scenarios.clear();
        Subcategory created{id, name, paletteColor(benchmark.categories.size() + category->subcategories.size()), {}};
        if (std::holds_alternative<RelocateDirectToNewSubcategory>(relocation))
            created.scenarios = std::move(direct);
        else
            for (auto &entry: direct) benchmark.uncategorized.push_back(std::move(entry));
        category->subcategories.push_back(std::move(created));
        return {std::nullopt, std::nullopt, id};
    }

    BenchmarkEditResult BenchmarkEditor::moveScenario(const ScenarioEntryId &id, const EditorGroupTarget &to) {
        return assignScenarios({id}, to);
    }

    BenchmarkEditResult BenchmarkEditor::appendUnnamedScenarios(std::size_t count) {
        BenchmarkEditResult result;
        for (std::size_t i = 0; i < count; ++i) {
            const auto id = ScenarioEntryId{newId()};
            m_target.uncategorized.push_back(ScenarioEntry{id, {}, std::nullopt, {}});
            result.createdEntries.push_back(id);
        }
        return result;
    }

    BenchmarkEditResult BenchmarkEditor::assignScenarios(const std::vector<ScenarioEntryId> &orderedIds,
                                                         const EditorGroupTarget &to) {
        auto &benchmark = m_target;
        if (const auto error = groupTargetError(benchmark, to)) return {error};
        std::vector<ScenarioEntryId> unique;
        for (const auto &id: orderedIds) {
            if (std::ranges::find(unique, id) != unique.end()) continue;
            if (!findEntry(benchmark, id)) return {BenchmarkEditError::UnknownEntry};
            unique.push_back(id);
        }
        std::vector<ScenarioEntry> moving;
        moving.reserve(unique.size());
        for (const auto &id: unique) moving.push_back(*detachEntry(benchmark, id));
        auto &destination = groupScenarios(benchmark, to);
        for (auto &entry: moving) destination.push_back(std::move(entry));
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::reorderSubcategory(const GroupId &id, std::size_t position) {
        for (auto &category: m_target.categories) {
            auto &subs = category.subcategories;
            const auto it = std::ranges::find_if(subs, [&](const Subcategory &sub) { return sub.id == id; });
            if (it == subs.end()) continue;
            if (!moveToPosition(subs, it, position)) return {BenchmarkEditError::PositionOutOfRange};
            return {};
        }
        return {BenchmarkEditError::UnknownGroup};
    }

    BenchmarkEditResult BenchmarkEditor::reorderScenario(const ScenarioEntryId &id, std::size_t position) {
        std::optional<BenchmarkEditError> error = BenchmarkEditError::UnknownEntry;
        forEachScenarioList(m_target, [&](std::vector<ScenarioEntry> &entries) {
            const auto it = std::ranges::find_if(entries, [&](const ScenarioEntry &entry) { return entry.id == id; });
            if (it == entries.end()) return false;
            if (moveToPosition(entries, it, position)) error.reset();
            else error = BenchmarkEditError::PositionOutOfRange;
            return true;
        });
        return {error};
    }
}
