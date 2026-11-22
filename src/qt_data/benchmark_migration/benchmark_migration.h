#pragma once

#include <QJsonObject>

#include <string>
#include <variant>

// Stored-data migration for benchmark files, kept apart from the current-format reader (ADR 0011).
// Each step turns one schema version's JSON into the next one's (ADR 0013) and never builds the
// in-memory model (ADR 0012): the store's schema 3 reader alone decides whether the result loads.
namespace ksv::qt_data::benchmark_migration {
    struct MigratedBenchmark {
        QJsonObject schema3;
        std::string storedName;
    };

    // The step `fromVersion` to `fromVersion + 1` found input not in its source format.
    struct BenchmarkMigrationMismatch {
        int fromVersion = 0;
    };

    // Runs every step from `version` (1 or 2) up to schema 3.
    [[nodiscard]] std::variant<MigratedBenchmark, BenchmarkMigrationMismatch> migrateBenchmark(
        const QJsonObject &stored, int version);
}
