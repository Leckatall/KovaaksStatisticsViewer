#include "benchmark_library_service.h"

namespace ksv::application {
    BenchmarkLibraryService::BenchmarkLibraryService(std::shared_ptr<IBenchmarkRepository> repository)
        : m_repository(std::move(repository)) {
        refresh();
    }

    void BenchmarkLibraryService::refresh() {
        const auto result = m_repository->scan();
        if (!result.snapshot) {
            // Directory-level failure: keep the last accepted snapshot; report the error.
            m_lastRefreshFailed = true;
            for (const auto &callback: m_callbacks) callback();
            return;
        }
        m_snapshot = result.snapshot;
        m_lastRefreshFailed = false;
        ++m_revision;
        for (const auto &callback: m_callbacks) callback();
    }
}
