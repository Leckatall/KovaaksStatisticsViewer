#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "benchmarks/benchmark.h"
#include "data/interfaces/benchmark_library_snapshot.h"

namespace ksv::data {
    // Opaque handle for an accepted loaded benchmark. Only BenchmarksService issues one, and only
    // from an entry currently in the accepted snapshot; `digest` is the content precondition the
    // store checks on the next replace or delete.
    struct BenchmarkEditToken {
        domain::BenchmarkId id;
        std::string filename;
        std::string digest;
    };

    enum class BenchmarkSaveError { EmptyName, UnknownTarget, StaleToken, Conflict, WriteFailed };

    struct BenchmarkSaveRequest {
        domain::Benchmark benchmark;
        std::optional<BenchmarkEditToken> token;  // nullopt requests a create; a token requests a replace
    };

    struct BenchmarkSaveOutcome {
        std::optional<BenchmarkEditToken> token;  // newly admitted token on success
        std::optional<BenchmarkSaveError> error;
        [[nodiscard]] bool ok() const { return token.has_value() && !error.has_value(); }
    };

    struct BenchmarkReplacementRequest {
        domain::Benchmark benchmark;
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
