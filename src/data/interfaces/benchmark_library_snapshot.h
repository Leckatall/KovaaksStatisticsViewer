#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "benchmarks/benchmark_definition.h"
#include "benchmarks/benchmark_validation.h"

// Payload shared by IBenchmarkStore::scan() and the accepted-library state IBenchmarksService
// publishes. It is data-owned vocabulary: presentation and application read it back but never
// author it.
namespace ksv::data {
    // A benchmark's name is its filename without this extension, matched ignoring ASCII case.
    inline constexpr std::string_view kBenchmarkFileExtension = ".json";

    enum class BenchmarkFileProblem { Invalid, Unsupported, MigrationFailed };

    enum class BenchmarkMigrationFailureReason { SourceMismatch, InvalidResult, WriteFailed, RenameFailed };

    // The step that failed is always `fromVersion` to `fromVersion + 1`.
    struct BenchmarkMigrationFailure {
        int fromVersion = 0;
        BenchmarkMigrationFailureReason reason = BenchmarkMigrationFailureReason::SourceMismatch;
        bool operator==(const BenchmarkMigrationFailure &) const = default;
    };

    struct LoadedBenchmark {
        domain::BenchmarkDefinition benchmark;
        domain::CompletenessResult completeness;
    };

    struct ProblemBenchmark {
        BenchmarkFileProblem problem;
        std::optional<int> schemaVersion;                            // populated for Unsupported
        std::optional<BenchmarkMigrationFailure> migrationFailure;  // populated for MigrationFailed
    };

    // One managed file. `filename` is the benchmark's identity and the snapshot key; `digest` is
    // the content precondition for later replacement or deletion.
    struct BenchmarkFileEntry {
        std::string filename;
        std::string digest;
        std::variant<LoadedBenchmark, ProblemBenchmark> content;
        // Filled by BenchmarksService, never by the store: a key minted per filename for this
        // session, also set as a loaded benchmark's `id`, and the benchmark name taken from the
        // filename, also set as a loaded benchmark's `name`.
        domain::BenchmarkId id;
        std::string name;
    };

    struct BenchmarkLibrarySnapshot {
        std::vector<BenchmarkFileEntry> entries;  // sorted by filename in std::string order
    };
}
