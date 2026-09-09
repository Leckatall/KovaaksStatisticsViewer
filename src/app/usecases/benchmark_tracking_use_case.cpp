#include "benchmark_tracking_use_case.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

#include "benchmarks/benchmark_evaluator.h"

namespace ksv::application {
    namespace {
        // Takes the snapshot by reference: IBenchmarkLibraryService::snapshot() returns by value,
        // so a pointer into a local copy would dangle the moment that copy died.
        const LoadedBenchmark *findLoaded(const BenchmarkLibrarySnapshot &snapshot,
                                          const domain::BenchmarkId &id) {
            const LoadedBenchmark *found = nullptr;
            for (const auto &entry: snapshot.entries) {
                const auto *loaded = std::get_if<LoadedBenchmark>(&entry.content);
                if (!loaded || !(loaded->benchmark.id == id)) continue;
                // Two files claiming one id resolve to unavailable, never to an arbitrary pick.
                if (found != nullptr) return nullptr;
                found = loaded;
            }
            return found;
        }
    }

    BenchmarkTrackingUseCase::BenchmarkTrackingUseCase(
        std::shared_ptr<IBenchmarkLibraryService> library,
        std::shared_ptr<IProfileService> profileService)
        : m_library(std::move(library)), m_profileService(std::move(profileService)) {
        m_library->onChanged([this] {
            m_cache.clear();
            refresh();
            notifyChanged();
        });
        m_profileService->onProfileChanged([this] {
            m_cache.clear();
            refresh();
            notifyChanged();
        });
    }

    void BenchmarkTrackingUseCase::select(const domain::BenchmarkId &id) {
        if (m_selected && *m_selected == id) return;
        m_selected = id;
        refresh();
        notifyChanged();
    }

    void BenchmarkTrackingUseCase::clearSelection() {
        if (!m_selected) return;
        m_selected.reset();
        refresh();
        notifyChanged();
    }

    void BenchmarkTrackingUseCase::notifyChanged() const {
        for (const auto &callback: m_callbacks) callback();
    }

    void BenchmarkTrackingUseCase::refresh() {
        if (!m_selected) {
            m_state = BenchmarkTrackingState::NoSelection;
            m_projection = {};
            return;
        }

        // Any library or profile change drops the whole cache, so a surviving entry is always
        // current for the definition it was built from -- presence is the only check a hit needs.
        if (const auto cached = m_cache.find(*m_selected); cached != m_cache.end()) {
            m_projection = cached->second;
            m_state = BenchmarkTrackingState::Ready;
            return;
        }

        const auto snapshot = m_library->snapshot();
        const auto *loaded = snapshot ? findLoaded(*snapshot, *m_selected) : nullptr;
        if (!loaded) {
            m_state = BenchmarkTrackingState::Unavailable;
            m_projection = {};
            return;
        }

        m_projection = evaluate(*loaded);
        m_state = BenchmarkTrackingState::Ready;
        // Only Ready results are cached, so a benchmark that reappears is evaluated rather than
        // served a remembered absence.
        m_cache[*m_selected] = m_projection;
    }

    domain::BenchmarkProjection BenchmarkTrackingUseCase::evaluate(const LoadedBenchmark &loaded) const {
        const auto resolutions = m_library->resolutionsFor(loaded.benchmark.id);
        // Unique resolved hashes only. The name is left empty deliberately: ScenarioId equality,
        // ordering and hashing all ignore it, so the profile lookup needs nothing else.
        std::vector<domain::ScenarioId> scenarios;
        std::unordered_set<std::string> hashes;
        for (const auto &resolution: resolutions) {
            if (!resolution.hash || !hashes.insert(*resolution.hash).second) continue;
            scenarios.push_back({{}, *resolution.hash});
        }
        return domain::evaluateBenchmark(loaded.benchmark, loaded.completeness, resolutions,
                                         m_profileService->getRunFacts(scenarios),
                                         m_profileService->getRollingTimeAverage(scenarios, kRollingWindowDays));
    }
}
