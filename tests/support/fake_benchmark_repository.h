#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_REPOSITORY_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_REPOSITORY_H

#include "data/interfaces/i_benchmark_repository.h"

namespace ksv::tests_support {
    // Scriptable repository: tests set `nextScan` and count calls; write/remove are no-ops that
    // record their arguments. Slice 1's service only calls scan().
    class FakeBenchmarkRepository final : public application::IBenchmarkRepository {
    public:
        application::BenchmarkScanResult nextScan{};
        mutable int scanCount = 0;
        std::string directory = "/fake/benchmarks";

        [[nodiscard]] application::BenchmarkScanResult scan() const override {
            ++scanCount;
            return nextScan;
        }
        application::BenchmarkWriteResult write(const domain::Benchmark &, const std::string &,
                                                const std::optional<std::string> &) override {
            return {};
        }
        application::BenchmarkDeleteResult remove(const std::string &, const std::string &) override {
            return {};
        }
        [[nodiscard]] std::string managedDirectoryPath() const override { return directory; }
    };
}

#endif
