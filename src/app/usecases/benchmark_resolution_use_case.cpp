#include "benchmark_resolution_use_case.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "benchmarks/benchmark_validation.h"

namespace ksv::application {
    namespace {
        // Visits every scenario list in draft order: uncategorized, then each category's direct
        // scenarios and each subcategory's scenarios. `fn` returns true to stop early.
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

        domain::ScenarioEntry *findEntry(domain::Benchmark &benchmark, const domain::ScenarioEntryId &id) {
            domain::ScenarioEntry *found = nullptr;
            forEachScenarioList(benchmark, [&](std::vector<domain::ScenarioEntry> &entries) {
                const auto it = std::ranges::find_if(
                    entries, [&](const domain::ScenarioEntry &entry) { return entry.id == id; });
                if (it == entries.end()) return false;
                found = &*it;
                return true;
            });
            return found;
        }

        struct ScenarioCatalogue {
            std::map<std::string, std::vector<domain::ScenarioCandidate>> byName;
            std::unordered_set<std::string> knownHashes;
        };

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

        AutomaticMappingWriteError toWriteError(std::optional<data::BenchmarkSaveError> error) {
            return error == data::BenchmarkSaveError::Conflict ? AutomaticMappingWriteError::Conflict
                                                              : AutomaticMappingWriteError::WriteFailed;
        }
    }

    BenchmarkResolutionUseCase::BenchmarkResolutionUseCase(
        std::shared_ptr<data::IBenchmarksService> benchmarks, std::shared_ptr<IProfileService> profile)
        : m_benchmarks(std::move(benchmarks)), m_profile(std::move(profile)) {
        m_benchmarks->onChanged([this] { reconcile(); });
        m_profile->onProfileChanged([this] { reconcile(); });
        reconcile();
    }

    void BenchmarkResolutionUseCase::setEditLease(std::optional<domain::BenchmarkId> benchmark) {
        if (m_editLease == benchmark) return;
        m_editLease = std::move(benchmark);
        // Releasing (or repointing) the lease reconciles the newly eligible benchmark against the
        // latest accepted definition and token, independent of the presentation copy just closed.
        reconcile();
    }

    void BenchmarkResolutionUseCase::publish() {
        for (const auto &callback: m_callbacks) callback();
    }

    void BenchmarkResolutionUseCase::reconcile() {
        if (m_reconciling) {
            m_reconcileAgain = true;
            return;
        }
        m_reconciling = true;
        do {
            m_reconcileAgain = false;
            runReconcilePass();
        } while (m_reconcileAgain);
        m_reconciling = false;
        publish();
    }

    std::vector<domain::ScenarioResolution> BenchmarkResolutionUseCase::resolve(
        const domain::Benchmark &benchmark) const {
        ScenarioCatalogue catalogue;
        for (const auto &scenario: m_profile->getScenarioList()) {
            catalogue.knownHashes.insert(scenario.hash);
            catalogue.byName[scenario.name].push_back(
                {scenario.hash, static_cast<int>(m_profile->getRunCount(scenario).value_or(0)),
                 m_profile->getLastRunTime(scenario)});
        }
        for (auto &[_, candidates]: catalogue.byName)
            std::ranges::sort(candidates, {}, &domain::ScenarioCandidate::hash);

        std::vector<domain::ScenarioResolution> resolutions;
        forEachScenarioList(benchmark, [&](const std::vector<domain::ScenarioEntry> &entries) {
            for (const auto &entry: entries) {
                domain::ScenarioResolution resolution{entry.id};
                if (entry.hash) {
                    resolution.hash = entry.hash;
                    resolution.state = catalogue.knownHashes.contains(*entry.hash)
                                           ? domain::ScenarioMatchState::Resolved
                                           : domain::ScenarioMatchState::MappedUnavailable;
                } else if (const auto found = catalogue.byName.find(entry.name);
                           found != catalogue.byName.end()) {
                    resolution.candidates = found->second;
                    resolution.state = found->second.size() == 1
                                           ? domain::ScenarioMatchState::AutoMappable
                                           : domain::ScenarioMatchState::Ambiguous;
                }
                resolutions.push_back(std::move(resolution));
            }
            return false;
        });
        return resolutions;
    }

    void BenchmarkResolutionUseCase::runReconcilePass() {
        BenchmarkResolutionSnapshot next;

        auto catalogueScenarios = m_profile->getScenarioList();
        std::ranges::sort(catalogueScenarios, [](const auto &left, const auto &right) {
            return left.name != right.name ? left.name < right.name : left.hash < right.hash;
        });
        next.scenarioCatalogue = catalogueScenarios;

        ScenarioCatalogue catalogue;
        for (const auto &scenario: m_profile->getScenarioList()) {
            catalogue.knownHashes.insert(scenario.hash);
            catalogue.byName[scenario.name].push_back(
                {scenario.hash, static_cast<int>(m_profile->getRunCount(scenario).value_or(0)),
                 m_profile->getLastRunTime(scenario)});
        }
        for (auto &[_, candidates]: catalogue.byName)
            std::ranges::sort(candidates, {}, &domain::ScenarioCandidate::hash);

        const auto snapshot = m_benchmarks->snapshot();
        std::vector<data::BenchmarkReplacementRequest> requests;
        std::vector<domain::BenchmarkId> requestOwners;
        if (snapshot) {
            for (const auto &entry: snapshot->entries) {
                const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content);
                if (!loaded) continue;
                next.resolutions[loaded->benchmark.id] = resolve(loaded->benchmark);

                // A leased benchmark keeps its derived state current but is held back from
                // application-initiated background persistence until the lease is released.
                if (m_editLease && *m_editLease == loaded->benchmark.id) continue;
                const auto mappings = computeAutoMappings(loaded->benchmark, catalogue.byName);
                if (mappings.empty()) continue;
                auto updated = loaded->benchmark;
                for (const auto &[id, hash]: mappings)
                    if (auto *target = findEntry(updated, id)) target->hash = hash;
                requests.push_back({std::move(updated),
                                    data::BenchmarkEditToken{loaded->benchmark.id, entry.filename,
                                                             entry.digest}});
                requestOwners.push_back(loaded->benchmark.id);
            }
        }

        m_snapshot = std::move(next);

        if (requests.empty()) return;
        const auto outcome = m_benchmarks->replaceBatch(requests);
        for (std::size_t i = 0; i < requestOwners.size() && i < outcome.outcomes.size(); ++i)
            if (!outcome.outcomes[i].ok())
                m_snapshot.automaticWriteFailures[requestOwners[i]] =
                    toWriteError(outcome.outcomes[i].error);
    }
}
