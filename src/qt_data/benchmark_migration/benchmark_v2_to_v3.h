#pragma once

#include <QJsonObject>

#include <optional>
#include <string>

namespace ksv::qt_data::benchmark_migration {
    struct BenchmarkV2ToV3Result {
        QJsonObject schema3;
        // The removed top-level `name`, which the file is renamed after; possibly empty.
        std::string storedName;
    };

    // Benchmark schema 2 to schema 3: the top-level `id` and `name` are removed, since the filename
    // is now both. std::nullopt when `v2` is not exactly a schema 2 document.
    [[nodiscard]] std::optional<BenchmarkV2ToV3Result> migrateV2ToV3(const QJsonObject &v2);
}
