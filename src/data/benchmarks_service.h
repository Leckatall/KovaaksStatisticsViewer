#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "data/interfaces/i_benchmark_store.h"
#include "data/interfaces/i_benchmarks_service.h"

namespace ksv::data {
    class BenchmarksService final : public IBenchmarksService {
    public:
        explicit BenchmarksService(std::shared_ptr<IBenchmarkStore> store);

        [[nodiscard]] std::optional<BenchmarkLibrarySnapshot> snapshot() const override { return m_snapshot; }
        [[nodiscard]] uint64_t revision() const override { return m_revision; }
        [[nodiscard]] bool lastRefreshFailed() const override { return m_lastRefreshFailed; }
        [[nodiscard]] std::string managedDirectoryPath() const override {
            return m_store->managedDirectoryPath();
        }

        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }
        void refresh() override;

        BenchmarkSaveOutcome save(const BenchmarkSaveRequest &request) override;
        BenchmarkReplacementBatchOutcome replaceBatch(
            const std::vector<BenchmarkReplacementRequest> &requests) override;
        BenchmarkRemoveOutcome remove(const BenchmarkEditToken &token) override;

    private:
        void notifyChanged();
        // Accepted loaded entry a token still addresses, or nullptr if the filename is unknown or
        // its digest / embedded id no longer matches.
        [[nodiscard]] const BenchmarkFileEntry *acceptedEntry(const BenchmarkEditToken &token) const;
        void upsertSnapshotEntry(const std::string &filename, const std::string &digest,
                                 LoadedBenchmark content);
        // Validate, conditionally write, and (on success) install one accepted benchmark without
        // publishing. `changed` is set true on any accepted mutation so the caller publishes once.
        BenchmarkSaveOutcome writeAccepted(const domain::Benchmark &benchmark,
                                           const std::optional<BenchmarkEditToken> &token,
                                           bool &changed);

        std::shared_ptr<IBenchmarkStore> m_store;
        std::optional<BenchmarkLibrarySnapshot> m_snapshot;
        uint64_t m_revision = 0;
        bool m_lastRefreshFailed = false;
        std::vector<std::function<void()>> m_callbacks;
    };
}
