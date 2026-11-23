#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "benchmarks/benchmark_definition.h"
#include "data/interfaces/benchmark_library_snapshot.h"

namespace ksv::data {
    // Opaque handle for an accepted loaded benchmark. Only BenchmarksService issues one, and only
    // from an entry currently in the accepted snapshot; `digest` is the content precondition the
    // store checks on the next replace or delete. `id` is the entry's in-session key.
    struct BenchmarkEditToken {
        domain::BenchmarkId id;
        std::string filename;
        std::string digest;
    };

    enum class BenchmarkSaveError { UnknownTarget, StaleToken, Conflict, WriteFailed, RenameFailed };

    // `benchmark.name` is the name to save under, which the caller has already checked is valid and
    // unused. A replace whose name differs from its file's renames the file after writing it.
    struct BenchmarkSaveRequest {
        domain::BenchmarkDefinition benchmark;
        std::optional<BenchmarkEditToken> token;  // nullopt requests a create; a token requests a replace
    };

    // RenameFailed is not ok() but carries the token for the content written under the old name.
    struct BenchmarkSaveOutcome {
        std::optional<BenchmarkEditToken> token;  // newly admitted token on success
        std::optional<BenchmarkSaveError> error;
        std::string name;  // the benchmark name the saved file now has
        [[nodiscard]] bool ok() const { return token.has_value() && !error.has_value(); }
    };

    // Rewrites the token's file in place: never renamed, never created.
    struct BenchmarkReplacementRequest {
        domain::BenchmarkDefinition benchmark;
        BenchmarkEditToken token;
    };

    struct BenchmarkReplacementBatchOutcome {
        std::vector<BenchmarkSaveOutcome> outcomes;  // one per request, in input order
    };

    enum class BenchmarkRemoveError { UnknownTarget, StaleToken, Conflict, WriteFailed };

    struct BenchmarkRemoveOutcome {
        std::optional<BenchmarkRemoveError> error;
        [[nodiscard]] bool ok() const { return !error.has_value(); }
    };

    // Qt-free authority over the accepted benchmark library: it owns the accepted snapshot, a
    // monotonic in-process revision, refresh diagnostics, and coherent change publication. It has
    // no dependency on profile state, presentation state, or any active editor's working copy.
    // A benchmark is identified by its filename. Each entry carries a key minted for its filename,
    // kept across refreshes while that filename is present and moved with the file by a renaming
    // save; a filename that disappears and comes back, or appears by an outside rename, is new.
    class IBenchmarksService {
    public:
        virtual ~IBenchmarksService() = default;

        [[nodiscard]] virtual std::optional<BenchmarkLibrarySnapshot> snapshot() const = 0;
        [[nodiscard]] virtual uint64_t revision() const = 0;
        [[nodiscard]] virtual bool lastRefreshFailed() const = 0;
        [[nodiscard]] virtual std::string managedDirectoryPath() const = 0;

        virtual void onChanged(std::function<void()> callback) = 0;
        virtual void refresh() = 0;

        virtual BenchmarkSaveOutcome save(const BenchmarkSaveRequest &request) = 0;
        virtual BenchmarkReplacementBatchOutcome replaceBatch(
            const std::vector<BenchmarkReplacementRequest> &requests) = 0;
        virtual BenchmarkRemoveOutcome remove(const BenchmarkEditToken &token) = 0;
    };
}
