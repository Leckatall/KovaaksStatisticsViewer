#include "benchmark_tracking_use_case.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

#include "benchmarks/benchmark_evaluator.h"

namespace ksv::application {
    namespace {
        // Takes the snapshot by reference: IBenchmarksService::snapshot() returns by value, so a
        // pointer into a local copy would dangle the moment that copy died.
        const data::LoadedBenchmark *findLoaded(const data::BenchmarkLibrarySnapshot &snapshot,
                                                const domain::BenchmarkId &id) {
            const data::LoadedBenchmark *found = nullptr;
            for (const auto &entry: snapshot.entries) {
                const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content);
                if (!loaded || !(loaded->benchmark.id == id)) continue;
                // Two files claiming one id resolve to unavailable, never to an arbitrary pick.
                if (found != nullptr) return nullptr;
                found = loaded;
            }
            return found;
        }

        BenchmarkChoice choiceFor(const data::BenchmarkFileEntry &entry) {
            BenchmarkChoice choice;
            choice.filename = entry.filename;
            if (const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content)) {
                choice.id = loaded->benchmark.id;
                choice.displayName = loaded->benchmark.name.empty() ? entry.filename : loaded->benchmark.name;
                choice.classification = loaded->completeness.completeness == domain::Completeness::Trackable
                                            ? BenchmarkChoiceClassification::Trackable
                                            : BenchmarkChoiceClassification::Incomplete;
                choice.selectable = true;
                return choice;
            }
            const auto &problem = std::get<data::ProblemBenchmark>(entry.content);
            choice.id = problem.id;
            choice.displayName = problem.displayName && !problem.displayName->empty()
                                     ? *problem.displayName
                                     : entry.filename;
            choice.classification = problem.problem == data::BenchmarkFileProblem::Unsupported
                                        ? BenchmarkChoiceClassification::Unsupported
                                        : BenchmarkChoiceClassification::Invalid;
            choice.schemaVersion = problem.schemaVersion;
            choice.selectable = false;
            return choice;
        }
    }

    BenchmarkTrackingUseCase::BenchmarkTrackingUseCase(
        std::shared_ptr<data::IBenchmarksService> benchmarks,
        std::shared_ptr<IBenchmarkResolutionUseCase> resolution,
        std::shared_ptr<IProfileService> profileService)
        : m_benchmarks(std::move(benchmarks)), m_resolution(std::move(resolution)),
          m_profileService(std::move(profileService)) {
        const auto reevaluate = [this] {
            m_cache.clear();
            refresh();
            notifyChanged();
        };
        m_benchmarks->onChanged(reevaluate);
        m_resolution->onChanged(reevaluate);
        m_profileService->onProfileChanged(reevaluate);
        refresh();
    }

    void BenchmarkTrackingUseCase::select(const domain::BenchmarkId &id) {
        if (m_selected && *m_selected == id) return;
        m_selected = id;
        m_lastKnownSelectedDisplayName.clear();
        refresh();
        notifyChanged();
    }

    void BenchmarkTrackingUseCase::clearSelection() {
        if (!m_selected) return;
        m_selected.reset();
        m_lastKnownSelectedDisplayName.clear();
        refresh();
        notifyChanged();
    }

    void BenchmarkTrackingUseCase::notifyChanged() const {
        for (const auto &callback: m_callbacks) callback();
    }

    void BenchmarkTrackingUseCase::refresh() {
        const auto revision = m_benchmarks->revision();
        if (!m_librarySnapshotValid || revision != m_librarySnapshotRevision) {
            m_librarySnapshot = m_benchmarks->snapshot();
            m_librarySnapshotRevision = revision;
            m_librarySnapshotValid = true;
        }
        const auto &library = m_librarySnapshot;

        m_snapshot = {};
        m_snapshot.libraryRevision = revision;
        m_snapshot.selectedId = m_selected;
        if (library) {
            for (const auto &entry: library->entries) m_snapshot.choices.push_back(choiceFor(entry));
        }

        if (!m_selected) {
            m_state = BenchmarkTrackingState::NoSelection;
            m_projection = {};
            m_snapshot.availability = m_state;
            return;
        }

        m_snapshot.lastKnownSelectedDisplayName = m_lastKnownSelectedDisplayName;
        for (const auto &choice: m_snapshot.choices) {
            if (choice.id == m_selected) {
                m_lastKnownSelectedDisplayName = choice.displayName;
                m_snapshot.lastKnownSelectedDisplayName = choice.displayName;
                break;
            }
        }

        const data::LoadedBenchmark *loaded = library ? findLoaded(*library, *m_selected) : nullptr;

        if (const auto cached = m_cache.find(*m_selected); cached != m_cache.end()) {
            // Any library or profile change drops the whole cache, so a surviving entry is always
            // current for the definition it was built from -- presence is the only check a hit needs.
            m_projection = cached->second;
            m_state = BenchmarkTrackingState::Ready;
        } else if (!loaded) {
            m_state = BenchmarkTrackingState::Unavailable;
            m_projection = {};
        } else {
            m_projection = evaluate(*loaded);
            m_state = BenchmarkTrackingState::Ready;
            // Only Ready results are cached, so a benchmark that reappears is evaluated rather than
            // served a remembered absence.
            m_cache[*m_selected] = m_projection;
        }

        m_snapshot.availability = m_state;
        if (m_state == BenchmarkTrackingState::Ready) {
            if (loaded) m_snapshot.selectedLoaded = *loaded;
            m_snapshot.projection = m_projection;
        }
    }

    domain::BenchmarkProjection BenchmarkTrackingUseCase::evaluate(const data::LoadedBenchmark &loaded) const {
        const auto resolutionSnapshot = m_resolution->snapshot();
        const auto entry = resolutionSnapshot.resolutions.find(loaded.benchmark.id);
        const std::vector<domain::ScenarioResolution> resolutions =
            entry != resolutionSnapshot.resolutions.end() ? entry->second
                                                          : std::vector<domain::ScenarioResolution>{};
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
