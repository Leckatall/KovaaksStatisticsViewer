#pragma once

#include <cstdint>
#include <functional>
#include <map>
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
        void publish();
        // Sets the entry's key and its name from the filename, onto a loaded benchmark too, which
        // is then re-validated: the store's definitions carry neither.
        static void identify(BenchmarkFileEntry &entry, const domain::BenchmarkId &key);
        [[nodiscard]] bool knows(const std::string &filename) const;
        // Accepted loaded entry a token still addresses, or nullptr if the filename is unknown or
        // its digest / key no longer matches.
        [[nodiscard]] const BenchmarkFileEntry *acceptedEntry(const BenchmarkEditToken &token) const;
        [[nodiscard]] std::optional<BenchmarkSaveError> refusal(const BenchmarkEditToken &token) const;
        // Installs written content under `filename` and `key` without publishing.
        void admit(const std::string &filename, const std::string &digest,
                   const domain::BenchmarkDefinition &benchmark, const domain::BenchmarkId &key);
        void drop(const std::string &filename);

        std::shared_ptr<IBenchmarkStore> m_store;
        std::optional<BenchmarkLibrarySnapshot> m_snapshot;
        // Key of every filename in m_snapshot.
        std::map<std::string, domain::BenchmarkId> m_keys;
        uint64_t m_nextKey = 0;
        uint64_t m_revision = 0;
        bool m_lastRefreshFailed = false;
        std::vector<std::function<void()>> m_callbacks;
    };
}
