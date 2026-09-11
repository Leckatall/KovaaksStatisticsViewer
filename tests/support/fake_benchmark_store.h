#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_STORE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_STORE_H

#include "data/interfaces/i_benchmark_store.h"

namespace ksv::tests_support {
    // Scriptable store: tests set `nextScan`/`nextWrite`/`nextRemove` and read back
    // the recorded call arguments.
    class FakeBenchmarkStore final : public data::IBenchmarkStore {
    public:
        data::BenchmarkScanResult nextScan{};
        // Consumed in order by successive scan() calls; nextScan is the fallback once exhausted.
        mutable std::vector<data::BenchmarkScanResult> scriptedScans;
        mutable int scanCount = 0;
        std::string directory = "/fake/benchmarks";

        data::BenchmarkWriteResult nextWrite{};
        // Consumed in order by successive write() calls; nextWrite is the fallback once exhausted.
        std::vector<data::BenchmarkWriteResult> scriptedWrites;
        data::BenchmarkDeleteResult nextRemove{};
        int writeCount = 0;
        int removeCount = 0;
        domain::Benchmark lastWritten;
        std::string lastWriteFilename;
        std::optional<std::string> lastWriteDigest;
        std::string lastRemoveFilename;
        std::string lastRemoveDigest;

        [[nodiscard]] data::BenchmarkScanResult scan() const override {
            const auto index = scanCount++;
            if (index < static_cast<int>(scriptedScans.size())) return scriptedScans[index];
            return nextScan;
        }
        data::BenchmarkWriteResult write(const domain::Benchmark &definition,
                                         const std::string &filename,
                                         const std::optional<std::string> &expectedDigest) override {
            const auto index = writeCount++;
            lastWritten = definition;
            lastWriteFilename = filename;
            lastWriteDigest = expectedDigest;
            if (index < static_cast<int>(scriptedWrites.size())) return scriptedWrites[index];
            return nextWrite;
        }
        data::BenchmarkDeleteResult remove(const std::string &filename,
                                           const std::string &expectedDigest) override {
            ++removeCount;
            lastRemoveFilename = filename;
            lastRemoveDigest = expectedDigest;
            return nextRemove;
        }
        [[nodiscard]] std::string managedDirectoryPath() const override { return directory; }
    };
}

#endif
