#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARKS_SERVICE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARKS_SERVICE_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "data/interfaces/i_benchmarks_service.h"

namespace ksv::tests_support {
    // Scriptable accepted-library service: tests set the observable state and the scripted
    // save/replace/remove outcomes, then call `fireChanged()` to drive subscribers.
    class FakeBenchmarksService final : public data::IBenchmarksService {
    public:
        std::optional<data::BenchmarkLibrarySnapshot> snapshotValue;
        uint64_t revisionValue = 0;
        bool refreshFailed = false;
        std::string directory = "/fake/benchmarks";

        std::vector<std::function<void()>> callbacks;
        int refreshCount = 0;

        data::BenchmarkSaveOutcome nextSaveOutcome{};
        std::vector<data::BenchmarkSaveRequest> saveRequests;

        data::BenchmarkReplacementBatchOutcome nextBatchOutcome{};
        std::vector<std::vector<data::BenchmarkReplacementRequest>> batchRequests;
        // Opt-in: on the first replaceBatch, install each request's benchmark into snapshotValue
        // (keyed by token filename), bump the revision, and fire subscribers synchronously from
        // inside the call — models a store that publishes its accepted replacement re-entrantly.
        bool republishReplacementsSynchronously = false;
        int replaceBatchCalls = 0;

        data::BenchmarkRemoveOutcome nextRemoveOutcome{};
        std::vector<data::BenchmarkEditToken> removeRequests;

        [[nodiscard]] std::optional<data::BenchmarkLibrarySnapshot> snapshot() const override {
            return snapshotValue;
        }
        [[nodiscard]] uint64_t revision() const override { return revisionValue; }
        [[nodiscard]] bool lastRefreshFailed() const override { return refreshFailed; }
        [[nodiscard]] std::string managedDirectoryPath() const override { return directory; }

        void onChanged(std::function<void()> callback) override {
            callbacks.push_back(std::move(callback));
        }
        void refresh() override { ++refreshCount; }

        data::BenchmarkSaveOutcome save(const data::BenchmarkSaveRequest &request) override {
            saveRequests.push_back(request);
            return nextSaveOutcome;
        }
        data::BenchmarkReplacementBatchOutcome replaceBatch(
            const std::vector<data::BenchmarkReplacementRequest> &requests) override {
            batchRequests.push_back(requests);
            if (republishReplacementsSynchronously && ++replaceBatchCalls == 1 && snapshotValue) {
                for (const auto &request: requests) {
                    for (auto &entry: snapshotValue->entries) {
                        if (entry.filename != request.token.filename) continue;
                        entry.digest += "+";
                        entry.content = data::LoadedBenchmark{
                            request.benchmark, domain::validateBenchmark(request.benchmark)};
                    }
                }
                ++revisionValue;
                fireChanged();
            }
            return nextBatchOutcome;
        }
        data::BenchmarkRemoveOutcome remove(const data::BenchmarkEditToken &token) override {
            removeRequests.push_back(token);
            return nextRemoveOutcome;
        }

        void fireChanged() {
            for (const auto &callback: callbacks) callback();
        }
    };
}

#endif
