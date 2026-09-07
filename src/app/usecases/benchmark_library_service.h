#pragma once

#include <memory>
#include <vector>

#include "contracts/i_benchmark_library_service.h"
#include "data/interfaces/i_benchmark_repository.h"

namespace ksv::application {
    class BenchmarkLibraryService final : public IBenchmarkLibraryService {
    public:
        explicit BenchmarkLibraryService(std::shared_ptr<IBenchmarkRepository> repository);

        [[nodiscard]] std::optional<BenchmarkLibrarySnapshot> snapshot() const override { return m_snapshot; }
        [[nodiscard]] uint64_t revision() const override { return m_revision; }
        [[nodiscard]] bool lastRefreshFailed() const override { return m_lastRefreshFailed; }
        void refresh() override;
        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }
        [[nodiscard]] std::string managedDirectoryPath() const override {
            return m_repository->managedDirectoryPath();
        }

    private:
        std::shared_ptr<IBenchmarkRepository> m_repository;
        std::optional<BenchmarkLibrarySnapshot> m_snapshot;
        uint64_t m_revision = 0;
        bool m_lastRefreshFailed = false;
        std::vector<std::function<void()>> m_callbacks;
    };
}
