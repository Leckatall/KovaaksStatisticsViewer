#include "benchmark_manager_use_case.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace ksv::application {
    namespace {
        // saveDraft() fires the service's library publication and its draft notification back to
        // back with nothing changing between them; the manager collapses that pair into one
        // onChanged by rebuilding and only notifying when a field its consumers observe actually
        // moved. libraryRevision carries library-snapshot identity (its coherence key), so the
        // full BenchmarkLibrarySnapshot never needs a deep compare here; every other field is
        // compared with its own value equality so a new field is picked up without editing this.
        bool sameState(const BenchmarkManagerState &a, const BenchmarkManagerState &b) {
            return a.libraryRevision == b.libraryRevision
                   && a.library.has_value() == b.library.has_value()
                   && a.refreshFailed == b.refreshFailed
                   && a.managedDirectoryPath == b.managedDirectoryPath
                   && a.draft == b.draft
                   && a.draftFromLibrary == b.draftFromLibrary
                   && a.draftDirty == b.draftDirty
                   && a.draftCompleteness == b.draftCompleteness
                   && a.scenarioCatalogue == b.scenarioCatalogue
                   && a.draftResolutions == b.draftResolutions
                   && a.resolutionWriteFailed == b.resolutionWriteFailed;
        }
    }

    BenchmarkManagerUseCase::BenchmarkManagerUseCase(std::shared_ptr<IBenchmarkLibraryService> library)
        : m_library(std::move(library)) {
        m_library->onChanged([this] { rebuildAndNotify(); });
        m_library->onDraftChanged([this] { rebuildAndNotify(); });
        m_state = build();
    }

    BenchmarkManagerState BenchmarkManagerUseCase::build() const {
        BenchmarkManagerState state;
        state.library = m_library->snapshot();
        state.libraryRevision = m_library->revision();
        state.refreshFailed = m_library->lastRefreshFailed();
        state.managedDirectoryPath = m_library->managedDirectoryPath();
        state.draft = m_library->draft();
        state.draftFromLibrary = m_library->draftFromLibrary();
        state.draftDirty = m_library->draftDirty();
        state.draftCompleteness = m_library->draftValidation();
        state.scenarioCatalogue = m_library->scenarioCatalogue();
        state.draftResolutions = m_library->draftResolutions();
        state.resolutionWriteFailed = m_library->lastResolutionWriteFailed();
        return state;
    }

    void BenchmarkManagerUseCase::rebuildAndNotify() {
        auto next = build();
        if (sameState(next, m_state)) return;
        m_state = std::move(next);
        for (const auto &callback: m_callbacks) callback();
    }
}
