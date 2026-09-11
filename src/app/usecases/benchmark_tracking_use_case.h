#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_USE_CASE_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_USE_CASE_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "contracts/benchmark_workspace_snapshot.h"
#include "contracts/i_benchmark_resolution_use_case.h"
#include "contracts/i_benchmark_tracking_use_case.h"
#include "data/interfaces/i_benchmarks_service.h"
#include "data/interfaces/i_profile_service.h"

namespace ksv::application {
    class BenchmarkTrackingUseCase final : public IBenchmarkTrackingUseCase {
    public:
        BenchmarkTrackingUseCase(std::shared_ptr<data::IBenchmarksService> benchmarks,
                                 std::shared_ptr<IBenchmarkResolutionUseCase> resolution,
                                 std::shared_ptr<IProfileService> profileService);

        void select(const domain::BenchmarkId &id) override;
        void clearSelection() override;
        [[nodiscard]] const BenchmarkWorkspaceSnapshot &snapshot() const override { return m_snapshot; }
        [[nodiscard]] std::optional<domain::BenchmarkId> selected() const override { return m_selected; }
        [[nodiscard]] BenchmarkTrackingState state() const override { return m_state; }
        [[nodiscard]] const domain::BenchmarkProjection *projection() const override {
            return m_state == BenchmarkTrackingState::Ready ? &m_projection : nullptr;
        }
        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }

    private:
        static constexpr int kRollingWindowDays = 3;

        void refresh();
        void notifyChanged() const;
        [[nodiscard]] domain::BenchmarkProjection evaluate(const data::LoadedBenchmark &loaded) const;

        std::shared_ptr<data::IBenchmarksService> m_benchmarks;
        std::shared_ptr<IBenchmarkResolutionUseCase> m_resolution;
        std::shared_ptr<IProfileService> m_profileService;
        // Cached whole-library snapshot; the service returns it by value (a deep copy of every
        // definition), so it is re-fetched only when the library revision moves, not on every
        // profile change that merely invalidates projections.
        std::optional<data::BenchmarkLibrarySnapshot> m_librarySnapshot;
        std::uint64_t m_librarySnapshotRevision = 0;
        bool m_librarySnapshotValid = false;
        std::optional<domain::BenchmarkId> m_selected;
        // Last display name m_selected resolved to from a choice; carried into the snapshot on
        // revisions where the selection has vanished or degraded to a problem with no parsed id,
        // so Unavailable can still name what the user picked.
        std::string m_lastKnownSelectedDisplayName;
        BenchmarkTrackingState m_state = BenchmarkTrackingState::NoSelection;
        domain::BenchmarkProjection m_projection;
        std::map<domain::BenchmarkId, domain::BenchmarkProjection> m_cache;
        BenchmarkWorkspaceSnapshot m_snapshot;
        std::vector<std::function<void()> > m_callbacks;
    };
}

#endif
