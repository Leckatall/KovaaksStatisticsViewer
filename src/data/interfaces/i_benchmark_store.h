#pragma once

#include <optional>
#include <string>

#include "benchmarks/benchmark.h"
#include "data/interfaces/benchmark_library_snapshot.h"

namespace ksv::data {
    enum class BenchmarkScanFailure { DirectoryUnavailable };

    struct BenchmarkScanResult {
        std::optional<BenchmarkLibrarySnapshot> snapshot;
        std::optional<BenchmarkScanFailure> failure;
    };

    enum class BenchmarkWriteFailure { ExternalModificationConflict, WriteFailed };

    struct BenchmarkWriteResult {
        std::optional<std::string> digest;  // new digest on success
        std::optional<BenchmarkWriteFailure> failure;
        [[nodiscard]] bool succeeded() const { return digest.has_value() && !failure.has_value(); }
    };

    struct BenchmarkDeleteResult {
        bool ok = false;
        std::optional<BenchmarkWriteFailure> failure;
    };

    // Stateless persistence port for the managed benchmark directory. It reports observations and
    // persistence outcomes; BenchmarksService alone decides whether an outcome changes accepted
    // memory. No accepted-state authority, no profile behavior, no editor lifecycle.
    class IBenchmarkStore {
    public:
        virtual ~IBenchmarkStore() = default;

        // Full re-enumeration + per-file classification. Directory-level failure leaves `snapshot`
        // empty and sets `failure`; the caller keeps its last accepted snapshot.
        [[nodiscard]] virtual BenchmarkScanResult scan() const = 0;

        // Atomic temp-file replacement. `expectedDigest` is nullopt for a new file, or the digest of
        // the bytes last admitted for `filename` when replacing; a mismatch is a conflict, not a write.
        virtual BenchmarkWriteResult write(const domain::Benchmark &definition,
                                           const std::string &filename,
                                           const std::optional<std::string> &expectedDigest) = 0;

        virtual BenchmarkDeleteResult remove(const std::string &filename,
                                             const std::string &expectedDigest) = 0;

        [[nodiscard]] virtual std::string managedDirectoryPath() const = 0;
    };
}
