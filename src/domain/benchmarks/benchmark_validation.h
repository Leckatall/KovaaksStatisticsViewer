#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_VALIDATION_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_VALIDATION_H

#include <variant>
#include <vector>

#include "benchmark.h"

namespace ksv::domain {
    enum class Completeness { Incomplete, Trackable };

    enum class BenchmarkIssueCode {
        MissingName,
        NoScenarios,
        NoTiers,
        DuplicateTierName,
        DuplicateScenarioMembership,  // two entries share a retained display name
        DuplicateResolvedHash,        // two entries resolve to the same hash
        MissingThreshold,             // an entry lacks a threshold for some tier
        DuplicateThreshold,           // an entry has two thresholds for one tier
        NonFiniteThreshold,
        NegativeThreshold,
        NonIncreasingThreshold,       // thresholds not strictly increasing in tier order
        EmptyCategory,                // user category with neither scenarios nor subcategories
        EmptySubcategory,
        MixedCategoryContent          // category holds both direct scenarios and subcategories
    };

    using IssueTarget = std::variant<std::monostate, BenchmarkId, TierId, GroupId, ScenarioEntryId>;

    struct BenchmarkIssue {
        BenchmarkIssueCode code;
        IssueTarget target;

        friend bool operator==(const BenchmarkIssue &, const BenchmarkIssue &) = default;
    };

    struct CompletenessResult {
        Completeness completeness = Completeness::Incomplete;
        std::vector<BenchmarkIssue> issues;

        friend bool operator==(const CompletenessResult &, const CompletenessResult &) = default;
    };

    [[nodiscard]] CompletenessResult validateBenchmark(const Benchmark &definition);
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_VALIDATION_H
