#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_REPOSITORY_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_REPOSITORY_H

#include "data/interfaces/i_benchmark_repository.h"

namespace ksv::tests_support {
    // Scriptable repository: tests set `nextScan`/`nextWrite`/`nextRemove` and read back
    // the recorded call arguments.
    class FakeBenchmarkRepository final : public application::IBenchmarkRepository {
    public:
        application::BenchmarkScanResult nextScan{};
        mutable int scanCount = 0;
        std::string directory = "/fake/benchmarks";

        application::BenchmarkWriteResult nextWrite{};
        application::BenchmarkDeleteResult nextRemove{};
        domain::Benchmark lastWritten;
        std::string lastWriteFilename;
        std::optional<std::string> lastWriteDigest;
        std::string lastRemoveFilename;

        [[nodiscard]] application::BenchmarkScanResult scan() const override {
            ++scanCount;
            return nextScan;
        }
        application::BenchmarkWriteResult write(const domain::Benchmark &definition,
                                                const std::string &filename,
                                                const std::optional<std::string> &expectedDigest) override {
            lastWritten = definition;
            lastWriteFilename = filename;
            lastWriteDigest = expectedDigest;
            return nextWrite;
        }
        application::BenchmarkDeleteResult remove(const std::string &filename,
                                                  const std::string &expectedDigest) override {
            lastRemoveFilename = filename;
            return nextRemove;
        }
        [[nodiscard]] std::string managedDirectoryPath() const override { return directory; }
    };
}

#endif
