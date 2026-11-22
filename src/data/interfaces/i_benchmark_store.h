#pragma once

#include <optional>
#include <string>

#include "benchmarks/benchmark_definition.h"
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

    struct BenchmarkRenameResult {
        bool ok = false;
        std::optional<BenchmarkWriteFailure> failure;
    };

    // Stateless persistence port for the managed benchmark directory. It reports observations and
    // persistence outcomes; BenchmarksService alone decides whether an outcome changes accepted
    // memory. No accepted-state authority, no profile behavior, no editor lifecycle.
    class IBenchmarkStore {
    public:
        virtual ~IBenchmarkStore() = default;

        // Full re-enumeration. Every file in an older stored format is migrated, renamed after its
        // stored name and rewritten first, so entries only ever hold the current format. A
        // directory-level failure leaves `snapshot` empty and sets `failure`; the caller keeps its
        // last accepted snapshot.
        [[nodiscard]] virtual BenchmarkScanResult scan() = 0;

        // Atomic temp-file replacement. `expectedDigest` is nullopt for a new file, or the digest of
        // the bytes last admitted for `filename` when replacing; a mismatch is a conflict, not a write.
        virtual BenchmarkWriteResult write(const domain::BenchmarkDefinition &definition,
                                           const std::string &filename,
                                           const std::optional<std::string> &expectedDigest) = 0;

        // A missing `from` or a digest mismatch is a conflict; an existing `to` that is another
        // file fails the rename. Changing only the letter case of `from` is allowed. The content,
        // and so its digest, is unchanged.
        virtual BenchmarkRenameResult rename(const std::string &from, const std::string &to,
                                             const std::string &expectedDigest) = 0;

        virtual BenchmarkDeleteResult remove(const std::string &filename,
                                             const std::string &expectedDigest) = 0;

        [[nodiscard]] virtual std::string managedDirectoryPath() const = 0;
    };
}
