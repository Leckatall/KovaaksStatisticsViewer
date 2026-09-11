#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "contracts/i_benchmark_resolution_use_case.h"
#include "data/interfaces/i_benchmarks_service.h"
#include "data/interfaces/i_profile_service.h"

namespace ksv::application {
    class BenchmarkResolutionUseCase final : public IBenchmarkResolutionUseCase {
    public:
        BenchmarkResolutionUseCase(std::shared_ptr<data::IBenchmarksService> benchmarks,
                                   std::shared_ptr<IProfileService> profile);

        [[nodiscard]] BenchmarkResolutionSnapshot snapshot() const override { return m_snapshot; }
        [[nodiscard]] std::vector<domain::ScenarioResolution> resolve(
            const domain::Benchmark &benchmark) const override;
        void setEditLease(std::optional<domain::BenchmarkId> benchmark) override;
        void onChanged(std::function<void()> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

    private:
        // Coalescing entry point for both change streams: a notification arriving while a pass runs
        // requests one more pass rather than recursing, so a data service that republishes its
        // accepted batch synchronously still settles to one publication.
        void reconcile();
        void runReconcilePass();
        void publish();

        std::shared_ptr<data::IBenchmarksService> m_benchmarks;
        std::shared_ptr<IProfileService> m_profile;
        BenchmarkResolutionSnapshot m_snapshot;
        std::optional<domain::BenchmarkId> m_editLease;
        std::vector<std::function<void()>> m_callbacks;
        bool m_reconciling = false;
        bool m_reconcileAgain = false;
    };
}
