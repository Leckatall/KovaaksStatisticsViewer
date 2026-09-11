#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_validation.h"

// Payload shared by IBenchmarkStore::scan() and the accepted-library state IBenchmarksService
// publishes. It is data-owned vocabulary: presentation and application read it back but never
// author it.
namespace ksv::data {
    enum class BenchmarkFileProblem { Invalid, Unsupported };

    struct LoadedBenchmark {
        domain::Benchmark benchmark;
        domain::CompletenessResult completeness;
    };

    struct ProblemBenchmark {
        BenchmarkFileProblem problem;
        std::optional<std::string> displayName;  // safe name if the field parsed
        std::optional<domain::BenchmarkId> id;    // embedded id if it parsed
        std::optional<int> schemaVersion;         // populated for Unsupported
    };

    // One managed file. `filename` (not the embedded id) is the snapshot key; `digest` is the
    // content precondition for later replacement or deletion.
    struct BenchmarkFileEntry {
        std::string filename;
        std::string digest;
        std::variant<LoadedBenchmark, ProblemBenchmark> content;
    };

    struct BenchmarkLibrarySnapshot {
        std::vector<BenchmarkFileEntry> entries;  // deterministic order, keyed by filename
    };
}
