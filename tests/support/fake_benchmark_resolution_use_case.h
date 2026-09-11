#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_RESOLUTION_USE_CASE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_RESOLUTION_USE_CASE_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "contracts/i_benchmark_resolution_use_case.h"

namespace ksv::tests_support {
    // Scriptable IBenchmarkResolutionUseCase: tests set `snapshotValue` and `resolveResult` (or
    // `resolveResultById` for per-input answers), read back the recorded `editLease` /
    // `resolvedBenchmarks`, and call `fireChanged()` to drive subscribers.
    class FakeBenchmarkResolutionUseCase final : public application::IBenchmarkResolutionUseCase {
    public:
        application::BenchmarkResolutionSnapshot snapshotValue;
        std::vector<domain::ScenarioResolution> resolveResult;
        std::map<std::string, std::vector<domain::ScenarioResolution>> resolveResultById;
        mutable std::vector<domain::Benchmark> resolvedBenchmarks;
        std::optional<domain::BenchmarkId> editLease;
        int setEditLeaseCount = 0;
        std::vector<std::function<void()>> callbacks;

        [[nodiscard]] application::BenchmarkResolutionSnapshot snapshot() const override {
            return snapshotValue;
        }
        [[nodiscard]] std::vector<domain::ScenarioResolution> resolve(
            const domain::Benchmark &benchmark) const override {
            resolvedBenchmarks.push_back(benchmark);
            if (const auto it = resolveResultById.find(benchmark.id.value); it != resolveResultById.end())
                return it->second;
            return resolveResult;
        }
        void setEditLease(std::optional<domain::BenchmarkId> benchmark) override {
            editLease = std::move(benchmark);
            ++setEditLeaseCount;
        }
        void onChanged(std::function<void()> callback) override {
            callbacks.push_back(std::move(callback));
        }

        void fireChanged() {
            for (const auto &callback: callbacks) callback();
        }
    };
}

#endif
