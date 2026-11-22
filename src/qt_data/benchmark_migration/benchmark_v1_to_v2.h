#pragma once

#include <QJsonObject>

#include <optional>

namespace ksv::qt_data::benchmark_migration {
    // Benchmark schema 1 to schema 2: each scenario entry's single `hash` becomes a `hashes` list.
    // std::nullopt when `v1` is not exactly a schema 1 document.
    [[nodiscard]] std::optional<QJsonObject> migrateV1ToV2(const QJsonObject &v1);
}
