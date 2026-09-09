#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_USE_CASE_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_TRACKING_USE_CASE_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "contracts/i_benchmark_library_service.h"
#include "contracts/i_benchmark_tracking_use_case.h"
#include "data/interfaces/i_profile_service.h"

namespace ksv::application {
    class BenchmarkTrackingUseCase final : public IBenchmarkTrackingUseCase {
    public:
        BenchmarkTrackingUseCase(std::shared_ptr<IBenchmarkLibraryService> library,
                                 std::shared_ptr<IProfileService> profileService);

        void select(const domain::BenchmarkId &id) override;
        void clearSelection() override;
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
        [[nodiscard]] domain::BenchmarkProjection evaluate(const LoadedBenchmark &loaded) const;

        std::shared_ptr<IBenchmarkLibraryService> m_library;
        std::shared_ptr<IProfileService> m_profileService;
        std::optional<domain::BenchmarkId> m_selected;
        BenchmarkTrackingState m_state = BenchmarkTrackingState::NoSelection;
        domain::BenchmarkProjection m_projection;
        std::map<domain::BenchmarkId, domain::BenchmarkProjection> m_cache;
        std::vector<std::function<void()> > m_callbacks;
    };
}

#endif
