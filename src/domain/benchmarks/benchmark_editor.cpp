#include "benchmark_editor.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace ksv::domain {
    namespace {
        struct EntryLocation {
            std::vector<BenchmarkScenario> *list = nullptr;
            std::vector<BenchmarkScenario>::iterator position;
        };

        EntryLocation locateEntry(BenchmarkDefinition &benchmark, const BenchmarkScenarioId &id) {
            for (auto *list: benchmark.scenarioLists()) {
                const auto it = std::ranges::find(*list, id, &BenchmarkScenario::id);
                if (it != list->end()) return {list, it};
            }
            return {};
        }

        Tier *findTier(BenchmarkDefinition &benchmark, const TierId &id) {
            const auto it = std::ranges::find_if(benchmark.tiers,
                                                 [&](const Tier &tier) { return tier.id == id; });
            return it == benchmark.tiers.end() ? nullptr : &*it;
        }

        Category *findCategory(BenchmarkDefinition &benchmark, const GroupId &id) {
            const auto it = std::ranges::find_if(benchmark.categories,
                                                 [&](const Category &category) { return category.id == id; });
            return it == benchmark.categories.end() ? nullptr : &*it;
        }

        BenchmarkGroup *findGroup(BenchmarkDefinition &benchmark, const GroupId &id) {
            if (auto *category = findCategory(benchmark, id)) return category;
            for (auto &category: benchmark.categories)
                for (auto &sub: category.subcategories)
                    if (sub.id == id) return &sub;
            return nullptr;
        }

        std::optional<BenchmarkScenario> detachEntry(BenchmarkDefinition &benchmark, const BenchmarkScenarioId &id) {
            const auto location = locateEntry(benchmark, id);
            if (!location.list) return std::nullopt;
            BenchmarkScenario detached = std::move(*location.position);
            location.list->erase(location.position);
            return detached;
        }

        bool nameExists(const BenchmarkDefinition &benchmark, const std::string &name) {
            return std::ranges::any_of(benchmark.entries(), [&](const BenchmarkScenario *entry) {
                return entry->name == name;
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
        std::optional<BenchmarkEditError> groupTargetError(BenchmarkDefinition &benchmark, const EditorGroupTarget &to) {
            const auto *targetGroup = std::get_if<GroupId>(&to);
            if (!targetGroup) return std::nullopt;
            if (!findGroup(benchmark, *targetGroup)) return BenchmarkEditError::UnknownGroup;
            return std::nullopt;
        }

        // `to` must already have passed groupTargetError().
        std::vector<BenchmarkScenario> &groupScenarios(BenchmarkDefinition &benchmark, const EditorGroupTarget &to) {
            const auto *targetGroup = std::get_if<GroupId>(&to);
            if (!targetGroup) return benchmark.uncategorized;
            return findGroup(benchmark, *targetGroup)->scenarios;
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
        for (auto *list: benchmark.scenarioLists())
            for (auto &entry: *list)
                std::erase_if(entry.thresholds, [&](const Threshold &threshold) { return threshold.tierId == id; });
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::addUnplayedScenario(const std::string &name) {
        if (nameExists(m_target, name)) return {BenchmarkEditError::DuplicateScenarioName};
        const auto id = BenchmarkScenarioId{newId()};
        m_target.uncategorized.push_back(BenchmarkScenario{id, name, {}, {}});
        return {std::nullopt, std::nullopt, std::nullopt, id};
    }

    BenchmarkEditResult BenchmarkEditor::addKnownScenario(const std::string &name, const std::string &hash) {
        if (nameExists(m_target, name)) return {BenchmarkEditError::DuplicateScenarioName};
        const auto id = BenchmarkScenarioId{newId()};
        m_target.uncategorized.push_back(BenchmarkScenario{id, name, hash.empty() ? std::vector<std::string>{} : std::vector{hash}, {}});
        return {std::nullopt, std::nullopt, std::nullopt, id};
    }

    BenchmarkEditResult BenchmarkEditor::renameScenario(const BenchmarkScenarioId &id, const std::string &name) {
        auto *entry = m_target.entry(id);
        if (!entry) return {BenchmarkEditError::UnknownEntry};
        entry->name = name;
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::setScenarioHash(const BenchmarkScenarioId &id,
                                                         const std::optional<std::string> &hash) {
        auto *entry = m_target.entry(id);
        if (!entry) return {BenchmarkEditError::UnknownEntry};
        entry->hashes.clear();
        if (hash && !hash->empty()) entry->hashes.push_back(*hash);
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::removeScenario(const BenchmarkScenarioId &id) {
        if (!detachEntry(m_target, id)) return {BenchmarkEditError::UnknownEntry};
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::setScenarioHashes(const BenchmarkScenarioId &id,
                                                           const std::vector<std::string> &hashes) {
        auto *entry = m_target.entry(id);
        if (!entry) return {BenchmarkEditError::UnknownEntry};
        entry->hashes.clear();
        for (const auto &hash: hashes)
            if (!hash.empty() && std::ranges::find(entry->hashes, hash) == entry->hashes.end())
                entry->hashes.push_back(hash);
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::setThreshold(const BenchmarkScenarioId &entryId,
                                                      const TierId &tierId, double score) {
        auto &benchmark = m_target;
        if (!findTier(benchmark, tierId)) return {BenchmarkEditError::UnknownTier};
        auto *entry = benchmark.entry(entryId);
        if (!entry) return {BenchmarkEditError::UnknownEntry};
        const auto existing = std::ranges::find_if(entry->thresholds,
                                                   [&](const Threshold &threshold) {
                                                       return threshold.tierId == tierId;
                                                   });
        if (existing != entry->thresholds.end()) existing->score = score;
        else entry->thresholds.push_back(Threshold{tierId, score});
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::clearThreshold(const BenchmarkScenarioId &entryId, const TierId &tierId) {
        if (!findTier(m_target, tierId)) return {BenchmarkEditError::UnknownTier};
        auto *entry = m_target.entry(entryId);
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
        auto *group = findGroup(m_target, id);
        if (!group) return {BenchmarkEditError::UnknownGroup};
        group->name = name;
        return {};
    }

    BenchmarkEditResult BenchmarkEditor::setGroupColor(const GroupId &id, BenchmarkColor color) {
        auto *group = findGroup(m_target, id);
        if (!group) return {BenchmarkEditError::UnknownGroup};
        group->color = color;
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
        auto &benchmark = m_target;
        auto *category = findCategory(benchmark, categoryId);
        if (!category) return {BenchmarkEditError::UnknownGroup};
        const auto id = GroupId{newId()};
        category->subcategories.push_back(
            Subcategory{id, name, paletteColor(benchmark.categories.size() + category->subcategories.size()), {}});
        return {std::nullopt, std::nullopt, id};
    }

    BenchmarkEditResult BenchmarkEditor::moveScenario(const BenchmarkScenarioId &id, const EditorGroupTarget &to) {
        return assignScenarios({id}, to);
    }

    BenchmarkEditResult BenchmarkEditor::appendUnnamedScenarios(std::size_t count) {
        BenchmarkEditResult result;
        for (std::size_t i = 0; i < count; ++i) {
            const auto id = BenchmarkScenarioId{newId()};
            m_target.uncategorized.push_back(BenchmarkScenario{id, {}, {}, {}});
            result.createdEntries.push_back(id);
        }
        return result;
    }

    BenchmarkEditResult BenchmarkEditor::assignScenarios(const std::vector<BenchmarkScenarioId> &orderedIds,
                                                         const EditorGroupTarget &to,
                                                         const std::optional<BenchmarkScenarioId> &before) {
        auto &benchmark = m_target;
        if (const auto error = groupTargetError(benchmark, to)) return {error};
        std::vector<BenchmarkScenarioId> unique;
        for (const auto &id: orderedIds) {
            if (std::ranges::find(unique, id) != unique.end()) continue;
            if (!benchmark.entry(id)) return {BenchmarkEditError::UnknownEntry};
            unique.push_back(id);
        }
        // Detaching only erases entries, never the containing vectors, so this stays valid.
        auto &destination = groupScenarios(benchmark, to);
        const auto entryIs = [](const BenchmarkScenarioId &id) {
            return [&id](const BenchmarkScenario &entry) { return entry.id == id; };
        };
        std::optional<BenchmarkScenarioId> anchor;
        if (before) {
            auto it = std::ranges::find_if(destination, entryIs(*before));
            if (it == destination.end()) return {BenchmarkEditError::PositionOutOfRange};
            it = std::find_if(it, destination.end(), [&](const BenchmarkScenario &entry) {
                return std::ranges::find(unique, entry.id) == unique.end();
            });
            if (it != destination.end()) anchor = it->id;
        }
        std::vector<BenchmarkScenario> moving;
        moving.reserve(unique.size());
        for (const auto &id: unique) moving.push_back(*detachEntry(benchmark, id));
        const auto at = anchor ? std::ranges::find_if(destination, entryIs(*anchor)) : destination.end();
        destination.insert(at, std::make_move_iterator(moving.begin()), std::make_move_iterator(moving.end()));
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

    BenchmarkEditResult BenchmarkEditor::reorderScenario(const BenchmarkScenarioId &id, std::size_t position) {
        const auto location = locateEntry(m_target, id);
        if (!location.list) return {BenchmarkEditError::UnknownEntry};
        if (!moveToPosition(*location.list, location.position, position)) return {BenchmarkEditError::PositionOutOfRange};
        return {};
    }
}
