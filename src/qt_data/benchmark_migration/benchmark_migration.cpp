#include "benchmark_migration.h"

#include "benchmark_v1_to_v2.h"
#include "benchmark_v2_to_v3.h"

namespace ksv::qt_data::benchmark_migration {
    std::variant<MigratedBenchmark, BenchmarkMigrationMismatch> migrateBenchmark(const QJsonObject &stored,
                                                                                 const int version) {
        auto current = stored;
        if (version <= 1) {
            const auto v2 = migrateV1ToV2(current);
            if (!v2) return BenchmarkMigrationMismatch{1};
            current = *v2;
        }
        const auto v3 = migrateV2ToV3(current);
        if (!v3) return BenchmarkMigrationMismatch{2};
        return MigratedBenchmark{v3->schema3, v3->storedName};
    }
}
