#include "benchmark_library_service.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <unordered_set>
#include <utility>

namespace ksv::application {
    namespace {
        bool isBlank(const std::string &text) {
            return std::ranges::all_of(text, [](unsigned char ch) { return std::isspace(ch) != 0; });
        }

        // Visits every scenario list in the hierarchy in draft order: uncategorized, then each
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

        domain::Tier *findTier(domain::Benchmark &benchmark, const domain::TierId &id) {
            const auto it = std::ranges::find_if(benchmark.tiers,
                                                 [&](const domain::Tier &tier) { return tier.id == id; });
            return it == benchmark.tiers.end() ? nullptr : &*it;
        }

        domain::ScenarioEntry *findEntry(domain::Benchmark &benchmark, const domain::ScenarioEntryId &id) {
            domain::ScenarioEntry *found = nullptr;
            forEachScenarioList(benchmark, [&](std::vector<domain::ScenarioEntry> &entries) {
                const auto it = std::ranges::find_if(entries,
                                                     [&](const domain::ScenarioEntry &entry) { return entry.id == id; });
                if (it == entries.end()) return false;
                found = &*it;
                return true;
            });
            return found;
        }

        domain::Category *findCategory(domain::Benchmark &benchmark, const domain::GroupId &id) {
            const auto it = std::ranges::find_if(benchmark.categories,
                                                 [&](const domain::Category &category) { return category.id == id; });
            return it == benchmark.categories.end() ? nullptr : &*it;
        }

        domain::Subcategory *findSubcategory(domain::Benchmark &benchmark, const domain::GroupId &id) {
            for (auto &category: benchmark.categories)
                for (auto &sub: category.subcategories)
                    if (sub.id == id) return &sub;
            return nullptr;
        }

        std::optional<domain::ScenarioEntry> detachEntry(domain::Benchmark &benchmark,
                                                         const domain::ScenarioEntryId &id) {
            std::optional<domain::ScenarioEntry> detached;
            forEachScenarioList(benchmark, [&](std::vector<domain::ScenarioEntry> &entries) {
                const auto it = std::ranges::find_if(entries,
                                                     [&](const domain::ScenarioEntry &entry) { return entry.id == id; });
                if (it == entries.end()) return false;
                detached = *it;
                entries.erase(it);
                return true;
            });
            return detached;
        }

        bool nameExists(const domain::Benchmark &benchmark, const std::string &name) {
            return forEachScenarioList(benchmark, [&](const std::vector<domain::ScenarioEntry> &entries) {
                return std::ranges::any_of(entries,
                                           [&](const domain::ScenarioEntry &entry) { return entry.name == name; });
            });
        }

        // KSV-assigned defaults for new tiers and groups; users may edit any of them.
        domain::BenchmarkColor paletteColor(size_t index) {
            static const domain::BenchmarkColor palette[] = {
                {205, 127, 50, 255},  {192, 192, 192, 255}, {255, 215, 0, 255}, {255, 140, 0, 255},
                {33, 150, 243, 255},  {76, 175, 80, 255},   {156, 39, 176, 255}, {233, 30, 99, 255},
            };
            return palette[index % std::size(palette)];
        }

        std::vector<std::pair<domain::ScenarioEntryId, std::string>> computeAutoMappings(
            const domain::Benchmark &benchmark,
            const std::map<std::string, std::vector<domain::ScenarioCandidate>> &byName) {
            std::unordered_set<std::string> taken;
            forEachScenarioList(benchmark, [&](const std::vector<domain::ScenarioEntry> &entries) {
                for (const auto &entry: entries)
                    if (entry.hash) taken.insert(*entry.hash);
                return false;
            });
            std::vector<std::pair<domain::ScenarioEntryId, std::string>> mappings;
            forEachScenarioList(benchmark, [&](const std::vector<domain::ScenarioEntry> &entries) {
                for (const auto &entry: entries) {
                    if (entry.hash) continue;
                    const auto found = byName.find(entry.name);
                    if (found == byName.end() || found->second.size() != 1) continue;
                    const auto &hash = found->second.front().hash;
                    if (taken.contains(hash)) continue;
                    taken.insert(hash);
                    mappings.push_back({entry.id, hash});
                }
                return false;
            });
            return mappings;
        }
    }

    BenchmarkLibraryService::BenchmarkLibraryService(std::shared_ptr<IBenchmarkRepository> repository,
                                                     std::shared_ptr<IPlaylistReader> playlistReader,
                                                     std::shared_ptr<IProfileService> profileService,
                                                     std::function<std::string()> idFactory)
        : m_repository(std::move(repository)),
          m_playlistReader(std::move(playlistReader)),
          m_idFactory(std::move(idFactory)),
          m_profileService(std::move(profileService)) {
        m_profileService->onProfileChanged([this] { reconcileFromProfile(); });
        refresh();
    }

    void BenchmarkLibraryService::refresh() {
        if (m_draft && m_draft->dirty) return;  // a dirty draft must not be silently superseded
        const auto result = m_repository->scan();
        if (!result.snapshot) {
            // Directory-level failure: keep the last accepted snapshot; report the error.
            m_lastRefreshFailed = true;
            for (const auto &callback: m_callbacks) callback();
            return;
        }
        m_snapshot = result.snapshot;
        m_lastRefreshFailed = false;
        reconcileInternal();
        publish();
    }

    BenchmarkLibraryService::ScenarioCatalogue BenchmarkLibraryService::buildCatalogue() const {
        ScenarioCatalogue catalogue;
        for (const auto &scenario: m_profileService->getScenarioList()) {
            catalogue.knownHashes.insert(scenario.hash);
            catalogue.byName[scenario.name].push_back(
                {scenario.hash, static_cast<int>(m_profileService->getRunCount(scenario).value_or(0)),
                 m_profileService->getLastRunTime(scenario)});
        }
        for (auto &[_, candidates]: catalogue.byName)
            std::ranges::sort(candidates, {}, &domain::ScenarioCandidate::hash);
        return catalogue;
    }

    std::vector<domain::ScenarioResolution> BenchmarkLibraryService::resolveAgainst(
        const domain::Benchmark &benchmark, const ScenarioCatalogue &catalogue) const {
        std::vector<domain::ScenarioResolution> resolutions;
        forEachScenarioList(benchmark, [&](const std::vector<domain::ScenarioEntry> &entries) {
            for (const auto &entry: entries) {
                domain::ScenarioResolution resolution{entry.id};
                if (entry.hash) {
                    resolution.hash = entry.hash;
                    resolution.state = catalogue.knownHashes.contains(*entry.hash)
                        ? domain::ScenarioMatchState::Resolved : domain::ScenarioMatchState::MappedUnavailable;
                } else if (const auto found = catalogue.byName.find(entry.name); found != catalogue.byName.end()) {
                    resolution.candidates = found->second;
                    resolution.state = found->second.size() == 1
                        ? domain::ScenarioMatchState::AutoMappable : domain::ScenarioMatchState::Ambiguous;
                }
                resolutions.push_back(std::move(resolution));
            }
            return false;
        });
        return resolutions;
    }

    std::vector<domain::ScenarioId> BenchmarkLibraryService::scenarioCatalogue() const {
        auto scenarios = m_profileService->getScenarioList();
        std::ranges::sort(scenarios, [](const auto &left, const auto &right) {
            return left.name != right.name ? left.name < right.name : left.hash < right.hash;
        });
        return scenarios;
    }

    std::vector<domain::ScenarioResolution> BenchmarkLibraryService::resolutionsFor(
        const domain::BenchmarkId &id) const {
        if (!m_snapshot) return {};
        const auto catalogue = buildCatalogue();
        for (const auto &entry: m_snapshot->entries) {
            const auto *loaded = std::get_if<LoadedBenchmark>(&entry.content);
            if (loaded && loaded->benchmark.id == id) return resolveAgainst(loaded->benchmark, catalogue);
        }
        return {};
    }

    std::vector<domain::ScenarioResolution> BenchmarkLibraryService::draftResolutions() const {
        return m_draft ? resolveAgainst(m_draft->benchmark, buildCatalogue())
                       : std::vector<domain::ScenarioResolution>{};
    }

    bool BenchmarkLibraryService::reconcileInternal() {
        if (!m_snapshot) return false;
        const auto catalogue = buildCatalogue();
        m_lastResolutionWriteFailed = false;
        struct PendingWrite { std::string filename; std::string digest; domain::Benchmark benchmark; };
        std::vector<PendingWrite> pending;
        for (const auto &entry: m_snapshot->entries) {
            const auto *loaded = std::get_if<LoadedBenchmark>(&entry.content);
            if (!loaded || (m_draft && m_draft->dirty && m_draft->benchmark.id == loaded->benchmark.id)) continue;
            const auto mappings = computeAutoMappings(loaded->benchmark, catalogue.byName);
            if (mappings.empty()) continue;
            auto updated = loaded->benchmark;
            for (const auto &[id, hash]: mappings)
                if (auto *target = findEntry(updated, id)) target->hash = hash;
            pending.push_back({entry.filename, entry.digest, std::move(updated)});
        }
        bool changed = false;
        for (auto &write: pending) {
            const auto result = m_repository->write(write.benchmark, write.filename, write.digest);
            if (!result.succeeded()) {
                m_lastResolutionWriteFailed = true;
                continue;
            }
            upsertSnapshotEntry(write.filename, *result.digest,
                LoadedBenchmark{write.benchmark, domain::validateBenchmark(write.benchmark)});
            changed = true;
        }
        return changed;
    }

    void BenchmarkLibraryService::syncDraftToSnapshot() {
        // Never touches a dirty draft. reconcileInternal() skips that benchmark's own file, but it
        // may still have written a different one and reported a change, and adopting the snapshot
        // here would silently discard the user's unsaved edits. saveDraft() clears the flag before
        // it calls this, so its own path is unaffected.
        if (!m_draft || m_draft->dirty || !m_draft->baselineFilename || !m_snapshot) return;
        for (const auto &entry: m_snapshot->entries) {
            if (entry.filename != *m_draft->baselineFilename) continue;
            const auto *loaded = std::get_if<LoadedBenchmark>(&entry.content);
            if (!loaded) return;
            m_draft->benchmark = loaded->benchmark;
            m_draft->baseline = loaded->benchmark;
            m_draft->baselineDigest = entry.digest;
            m_draft->validation = loaded->completeness;
            m_draft->dirty = false;
            return;
        }
    }

    void BenchmarkLibraryService::reconcileFromProfile() {
        if (reconcileInternal() && m_draft) {
            // A clean draft open on a benchmark whose file was just rewritten must adopt the new
            // content and digest, or its next save fails the content precondition against a change
            // this service itself made.
            syncDraftToSnapshot();
            notifyDraftChanged();
        }
        // Published even when no file changed: resolution state is derived from the profile, so a
        // hash appearing or a second same-name scenario arriving changes what the library reports
        // without touching a single byte on disk.
        publish();
    }

    void BenchmarkLibraryService::publish() {
        ++m_revision;
        for (const auto &callback: m_callbacks) callback();
    }

    void BenchmarkLibraryService::notifyDraftChanged() {
        for (const auto &callback: m_draftCallbacks) callback();
    }

    void BenchmarkLibraryService::revalidateDraft() {
        if (!m_draft) return;
        m_draft->validation = domain::validateBenchmark(m_draft->benchmark);
        m_draft->dirty = m_draft->benchmark != m_draft->baseline;
        notifyDraftChanged();
    }

    void BenchmarkLibraryService::beginNewDraft() {
        domain::Benchmark fresh;
        fresh.id = domain::BenchmarkId{newId()};
        m_draft = Draft{fresh, std::nullopt, std::nullopt, fresh, domain::validateBenchmark(fresh), true};
        notifyDraftChanged();
    }

    bool BenchmarkLibraryService::openDraft(const domain::BenchmarkId &id) {
        if (!m_snapshot) return false;
        for (const auto &entry: m_snapshot->entries) {
            const auto *loaded = std::get_if<LoadedBenchmark>(&entry.content);
            if (!loaded || !(loaded->benchmark.id == id)) continue;
            m_draft = Draft{loaded->benchmark, entry.filename, entry.digest,
                            loaded->benchmark, loaded->completeness, false};
            notifyDraftChanged();
            return true;
        }
        return false;
    }

    void BenchmarkLibraryService::discardDraft() {
        m_draft.reset();
        notifyDraftChanged();
        if (reconcileInternal()) publish();
    }

    PlaylistImportResult BenchmarkLibraryService::importPlaylist(const std::string &path) {
        const auto read = m_playlistReader->read(path);
        if (!read.seed) {
            // The port contract is seed-xor-failure; a port returning neither is treated as
            // "no usable scenario list" rather than as a phantom success.
            return {read.failure ? read.failure
                                 : std::optional<PlaylistImportFailure>{PlaylistImportFailure::NoUsableScenarioList},
                    {}};
        }
        domain::Benchmark imported;
        imported.id = domain::BenchmarkId{newId()};
        if (read.seed->name) imported.name = *read.seed->name;
        for (const auto &scenarioName: read.seed->scenarioNames)
            imported.uncategorized.push_back(domain::ScenarioEntry{
                domain::ScenarioEntryId{newId()}, scenarioName, std::nullopt, {}});
        m_draft = Draft{imported, std::nullopt, std::nullopt, imported,
                        domain::validateBenchmark(imported), true};
        notifyDraftChanged();
        return {std::nullopt, read.seed->skippedDuplicateIndices};
    }

    BenchmarkDraftResult BenchmarkLibraryService::renameBenchmark(const std::string &name) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        m_draft->benchmark.name = name;
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::addTier(const std::string &name) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        const auto id = domain::TierId{newId()};
        m_draft->benchmark.tiers.push_back(
            domain::Tier{id, name, paletteColor(m_draft->benchmark.tiers.size())});
        revalidateDraft();
        return {std::nullopt, id};
    }

    BenchmarkDraftResult BenchmarkLibraryService::renameTier(const domain::TierId &id, const std::string &name) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto *tier = findTier(m_draft->benchmark, id);
        if (!tier) return {BenchmarkDraftError::UnknownTier};
        tier->name = name;
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::setTierColor(const domain::TierId &id,
                                                               domain::BenchmarkColor color) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto *tier = findTier(m_draft->benchmark, id);
        if (!tier) return {BenchmarkDraftError::UnknownTier};
        tier->color = color;
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::reorderTier(const domain::TierId &id, size_t position) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &tiers = m_draft->benchmark.tiers;
        const auto it = std::ranges::find_if(tiers, [&](const domain::Tier &tier) { return tier.id == id; });
        if (it == tiers.end()) return {BenchmarkDraftError::UnknownTier};
        if (position >= tiers.size()) return {BenchmarkDraftError::PositionOutOfRange};
        const domain::Tier tier = *it;
        tiers.erase(it);
        tiers.insert(tiers.begin() + static_cast<std::ptrdiff_t>(position), tier);
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::removeTier(const domain::TierId &id) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &benchmark = m_draft->benchmark;
        const auto tierIt = std::ranges::find_if(benchmark.tiers,
                                                 [&](const domain::Tier &tier) { return tier.id == id; });
        if (tierIt == benchmark.tiers.end()) return {BenchmarkDraftError::UnknownTier};
        benchmark.tiers.erase(tierIt);
        forEachScenarioList(benchmark, [&](std::vector<domain::ScenarioEntry> &entries) {
            for (auto &entry: entries)
                std::erase_if(entry.thresholds,
                              [&](const domain::Threshold &threshold) { return threshold.tierId == id; });
            return false;
        });
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::addUnplayedScenario(const std::string &name) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        if (nameExists(m_draft->benchmark, name)) return {BenchmarkDraftError::DuplicateScenarioName};
        const auto id = domain::ScenarioEntryId{newId()};
        m_draft->benchmark.uncategorized.push_back(domain::ScenarioEntry{id, name, std::nullopt, {}});
        revalidateDraft();
        return {std::nullopt, std::nullopt, std::nullopt, id};
    }

    BenchmarkDraftResult BenchmarkLibraryService::addKnownScenario(const std::string &name,
                                                                   const std::string &hash) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        if (nameExists(m_draft->benchmark, name)) return {BenchmarkDraftError::DuplicateScenarioName};
        const auto id = domain::ScenarioEntryId{newId()};
        m_draft->benchmark.uncategorized.push_back(domain::ScenarioEntry{id, name, hash, {}});
        revalidateDraft();
        return {std::nullopt, std::nullopt, std::nullopt, id};
    }

    BenchmarkDraftResult BenchmarkLibraryService::renameScenario(const domain::ScenarioEntryId &id,
                                                                 const std::string &name) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto *entry = findEntry(m_draft->benchmark, id);
        if (!entry) return {BenchmarkDraftError::UnknownEntry};
        entry->name = name;
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::setScenarioHash(
        const domain::ScenarioEntryId &id, const std::optional<std::string> &hash) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto *entry = findEntry(m_draft->benchmark, id);
        if (!entry) return {BenchmarkDraftError::UnknownEntry};
        entry->hash = hash;
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::removeScenario(const domain::ScenarioEntryId &id) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        if (!detachEntry(m_draft->benchmark, id)) return {BenchmarkDraftError::UnknownEntry};
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::setThreshold(const domain::ScenarioEntryId &entryId,
                                                               const domain::TierId &tierId, double score) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &benchmark = m_draft->benchmark;
        if (!findTier(benchmark, tierId)) return {BenchmarkDraftError::UnknownTier};
        auto *entry = findEntry(benchmark, entryId);
        if (!entry) return {BenchmarkDraftError::UnknownEntry};
        const auto existing = std::ranges::find_if(entry->thresholds,
                                                   [&](const domain::Threshold &threshold) {
                                                       return threshold.tierId == tierId;
                                                   });
        if (existing != entry->thresholds.end()) existing->score = score;
        else entry->thresholds.push_back(domain::Threshold{tierId, score});
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::clearThreshold(const domain::ScenarioEntryId &entryId,
                                                                 const domain::TierId &tierId) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto *entry = findEntry(m_draft->benchmark, entryId);
        if (!entry) return {BenchmarkDraftError::UnknownEntry};
        std::erase_if(entry->thresholds,
                      [&](const domain::Threshold &threshold) { return threshold.tierId == tierId; });
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::addCategory(const std::string &name) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        const auto id = domain::GroupId{newId()};
        m_draft->benchmark.categories.push_back(
            domain::Category{id, name, paletteColor(m_draft->benchmark.categories.size()), {}, {}});
        revalidateDraft();
        return {std::nullopt, std::nullopt, id};
    }

    BenchmarkDraftResult BenchmarkLibraryService::renameGroup(const domain::GroupId &id, const std::string &name) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &benchmark = m_draft->benchmark;
        if (auto *category = findCategory(benchmark, id)) category->name = name;
        else if (auto *sub = findSubcategory(benchmark, id)) sub->name = name;
        else return {BenchmarkDraftError::UnknownGroup};
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::setGroupColor(const domain::GroupId &id,
                                                                domain::BenchmarkColor color) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &benchmark = m_draft->benchmark;
        if (auto *category = findCategory(benchmark, id)) category->color = color;
        else if (auto *sub = findSubcategory(benchmark, id)) sub->color = color;
        else return {BenchmarkDraftError::UnknownGroup};
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::reorderCategory(const domain::GroupId &id, size_t position) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &categories = m_draft->benchmark.categories;
        const auto it = std::ranges::find_if(categories,
                                             [&](const domain::Category &category) { return category.id == id; });
        if (it == categories.end()) return {BenchmarkDraftError::UnknownGroup};
        if (position >= categories.size()) return {BenchmarkDraftError::PositionOutOfRange};
        const domain::Category category = *it;
        categories.erase(it);
        categories.insert(categories.begin() + static_cast<std::ptrdiff_t>(position), category);
        revalidateDraft();
        return {};
    }

    BenchmarkDraftResult BenchmarkLibraryService::removeCategory(const domain::GroupId &id) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &benchmark = m_draft->benchmark;
        const auto it = std::ranges::find_if(benchmark.categories,
                                             [&](const domain::Category &category) { return category.id == id; });
        if (it != benchmark.categories.end()) {
            for (auto &entry: it->scenarios) benchmark.uncategorized.push_back(std::move(entry));
            for (auto &sub: it->subcategories)
                for (auto &entry: sub.scenarios) benchmark.uncategorized.push_back(std::move(entry));
            benchmark.categories.erase(it);
            revalidateDraft();
            return {};
        }
        // A subcategory's scenarios go back to Uncategorized: the parent may still hold other
        // subcategories, so re-homing them as direct scenarios could create a mixed shape.
        for (auto &category: benchmark.categories) {
            const auto subIt = std::ranges::find_if(category.subcategories,
                                                    [&](const domain::Subcategory &sub) { return sub.id == id; });
            if (subIt == category.subcategories.end()) continue;
            for (auto &entry: subIt->scenarios) benchmark.uncategorized.push_back(std::move(entry));
            category.subcategories.erase(subIt);
            revalidateDraft();
            return {};
        }
        return {BenchmarkDraftError::UnknownGroup};
    }

    BenchmarkDraftResult BenchmarkLibraryService::addSubcategory(const domain::GroupId &categoryId,
                                                                 const std::string &name) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &benchmark = m_draft->benchmark;
        auto *category = findCategory(benchmark, categoryId);
        if (!category) return {BenchmarkDraftError::UnknownGroup};
        const auto requestedId = domain::GroupId{newId()};
        if (!category->scenarios.empty()) {
            // Atomic reorganization: the existing direct scenarios land in one new
            // subcategory, so the category never ends up in a mixed shape.
            category->subcategories.push_back(domain::Subcategory{
                domain::GroupId{newId()}, category->name,
                paletteColor(benchmark.categories.size() + category->subcategories.size()),
                std::move(category->scenarios)});
            category->scenarios.clear();
        }
        category->subcategories.push_back(domain::Subcategory{
            requestedId, name,
            paletteColor(benchmark.categories.size() + category->subcategories.size()), {}});
        revalidateDraft();
        return {std::nullopt, std::nullopt, requestedId};
    }

    BenchmarkDraftResult BenchmarkLibraryService::moveScenario(const domain::ScenarioEntryId &id,
                                                               const DraftGroupTarget &to) {
        if (!m_draft) return {BenchmarkDraftError::NoDraft};
        auto &benchmark = m_draft->benchmark;
        const auto *targetGroup = std::get_if<domain::GroupId>(&to);
        if (targetGroup) {
            // Validate before detaching so a rejected move leaves the entry in place.
            const auto *category = findCategory(benchmark, *targetGroup);
            if (!category && !findSubcategory(benchmark, *targetGroup))
                return {BenchmarkDraftError::UnknownGroup};
            if (category && !category->subcategories.empty()) return {BenchmarkDraftError::MixedContent};
        }
        auto entry = detachEntry(benchmark, id);
        if (!entry) return {BenchmarkDraftError::UnknownEntry};
        if (!targetGroup) {
            benchmark.uncategorized.push_back(std::move(*entry));
        } else if (auto *category = findCategory(benchmark, *targetGroup)) {
            category->scenarios.push_back(std::move(*entry));
        } else {
            findSubcategory(benchmark, *targetGroup)->scenarios.push_back(std::move(*entry));
        }
        revalidateDraft();
        return {};
    }

    BenchmarkSaveResult BenchmarkLibraryService::saveDraft() {
        if (!m_draft) return {BenchmarkSaveError::NoDraft};
        if (isBlank(m_draft->benchmark.name)) return {BenchmarkSaveError::EmptyName};
        const std::string filename = m_draft->baselineFilename.value_or(m_draft->benchmark.id.value + ".json");
        const auto result = m_repository->write(m_draft->benchmark, filename, m_draft->baselineDigest);
        // succeeded() is digest-present AND failure-absent: a port that reports neither has not
        // written anything, so it must not be mistaken for a save that produced no digest.
        if (!result.succeeded()) {
            return {result.failure == BenchmarkWriteFailure::ExternalModificationConflict
                        ? std::optional{BenchmarkSaveError::Conflict}
                        : std::optional{BenchmarkSaveError::WriteFailed}};
        }
        upsertSnapshotEntry(filename, *result.digest, LoadedBenchmark{m_draft->benchmark, m_draft->validation});
        m_draft->baselineFilename = filename;
        m_draft->baselineDigest = *result.digest;
        m_draft->baseline = m_draft->benchmark;
        m_draft->dirty = false;
        if (reconcileInternal()) syncDraftToSnapshot();
        publish();
        notifyDraftChanged();
        return {};
    }

    BenchmarkDeleteOutcome BenchmarkLibraryService::deleteBenchmark(const domain::BenchmarkId &id) {
        if (!m_snapshot) return {BenchmarkDeleteError::NotFound};
        auto &entries = m_snapshot->entries;
        const auto it = std::ranges::find_if(entries, [&](const BenchmarkFileEntry &entry) {
            const auto *loaded = std::get_if<LoadedBenchmark>(&entry.content);
            return loaded && loaded->benchmark.id == id;
        });
        if (it == entries.end()) return {BenchmarkDeleteError::NotFound};
        const auto result = m_repository->remove(it->filename, it->digest);
        // `ok` is the success field: a port that declines without classifying a failure has left
        // the file on disk, so the snapshot entry must survive.
        if (!result.ok) {
            return {result.failure == BenchmarkWriteFailure::ExternalModificationConflict
                        ? std::optional{BenchmarkDeleteError::Conflict}
                        : std::optional{BenchmarkDeleteError::WriteFailed}};
        }
        entries.erase(it);
        publish();
        if (m_draft && m_draft->benchmark.id == id) discardDraft();
        return {};
    }

    void BenchmarkLibraryService::upsertSnapshotEntry(const std::string &filename, const std::string &digest,
                                                      LoadedBenchmark content) {
        if (!m_snapshot) m_snapshot = BenchmarkLibrarySnapshot{};
        auto &entries = m_snapshot->entries;
        const auto it = std::ranges::lower_bound(entries, filename, {}, &BenchmarkFileEntry::filename);
        if (it != entries.end() && it->filename == filename) {
            it->digest = digest;
            it->content = std::move(content);
            return;
        }
        entries.insert(it, BenchmarkFileEntry{filename, digest, std::move(content)});
    }
}
