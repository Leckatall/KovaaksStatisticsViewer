#pragma once

#include <QJsonObject>

#include <optional>
#include <string>
#include <variant>

// Stored-data migration for benchmark files, kept apart from the current-format reader (ADR 0011).
// Each step turns one schema version's JSON into the next one's (ADR 0013) and never builds the
// in-memory model (ADR 0012): the store's schema 3 reader alone decides whether the result loads.
namespace ksv::qt_data::benchmark_migration {
    struct MigratedBenchmark {
        QJsonObject schema3;
        // The removed top-level `name`, which the file is renamed after; possibly empty.
        std::string storedName;
    };

    // The step `fromVersion` to `fromVersion + 1` found input not in its source format.
    struct BenchmarkMigrationMismatch {
        int fromVersion = 0;
    };

    // Schema 1 to schema 2: each scenario entry's single `hash` becomes a `hashes` list.
    // std::nullopt when `v1` is not exactly a schema 1 document.
    [[nodiscard]] std::optional<QJsonObject> migrateV1ToV2(const QJsonObject &v1);

    // Schema 2 to schema 3: the top-level `id` and `name` are removed, since the filename is now
    // both. std::nullopt when `v2` is not exactly a schema 2 document.
    [[nodiscard]] std::optional<MigratedBenchmark> migrateV2ToV3(const QJsonObject &v2);

    // Runs every step from `version` (1 or 2) up to schema 3.
    [[nodiscard]] std::variant<MigratedBenchmark, BenchmarkMigrationMismatch> migrateBenchmark(
        const QJsonObject &stored, int version);
}
