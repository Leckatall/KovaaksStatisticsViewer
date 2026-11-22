#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_STORE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_BENCHMARK_STORE_H

#include "data/interfaces/i_benchmark_store.h"

namespace ksv::tests_support {
    // Scriptable store: tests set `nextScan`/`nextWrite`/`nextRename`/`nextRemove` and read back
    // the recorded call arguments.
    class FakeBenchmarkStore final : public data::IBenchmarkStore {
    public:
        data::BenchmarkScanResult nextScan{};
        // Consumed in order by successive scan() calls; nextScan is the fallback once exhausted.
        std::vector<data::BenchmarkScanResult> scriptedScans;
        int scanCount = 0;
        std::string directory = "/fake/benchmarks";

        data::BenchmarkWriteResult nextWrite{};
        // Consumed in order by successive write() calls; nextWrite is the fallback once exhausted.
        std::vector<data::BenchmarkWriteResult> scriptedWrites;
        data::BenchmarkRenameResult nextRename{true, std::nullopt};
        // Consumed in order by successive rename() calls; nextRename is the fallback once exhausted.
        std::vector<data::BenchmarkRenameResult> scriptedRenames;
        data::BenchmarkDeleteResult nextRemove{};
        int writeCount = 0;
        int renameCount = 0;
        int removeCount = 0;
        domain::BenchmarkDefinition lastWritten;
        std::string lastWriteFilename;
        std::optional<std::string> lastWriteDigest;
        std::string lastRenameFrom;
        std::string lastRenameTo;
        std::string lastRenameDigest;
        std::string lastRemoveFilename;
        std::string lastRemoveDigest;

        [[nodiscard]] data::BenchmarkScanResult scan() override {
            const auto index = scanCount++;
            if (index < static_cast<int>(scriptedScans.size())) return scriptedScans[index];
            return nextScan;
        }
        data::BenchmarkWriteResult write(const domain::BenchmarkDefinition &definition,
                                         const std::string &filename,
                                         const std::optional<std::string> &expectedDigest) override {
            const auto index = writeCount++;
            lastWritten = definition;
            lastWriteFilename = filename;
            lastWriteDigest = expectedDigest;
            if (index < static_cast<int>(scriptedWrites.size())) return scriptedWrites[index];
            return nextWrite;
        }
        data::BenchmarkRenameResult rename(const std::string &from, const std::string &to,
                                           const std::string &expectedDigest) override {
            const auto index = renameCount++;
            lastRenameFrom = from;
            lastRenameTo = to;
            lastRenameDigest = expectedDigest;
            if (index < static_cast<int>(scriptedRenames.size())) return scriptedRenames[index];
            return nextRename;
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
