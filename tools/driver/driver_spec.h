#pragma once

#include <QJsonObject>
#include <QString>
#include <optional>
#include <string>
#include <vector>

#include <domain/user_profile.h>

#include "benchmarks/benchmark.h"
#include "gallery_dataset.h"

namespace ksv::driver {
    // A generated benchmark: tiers placed at `factor` x each scenario's personal
    // best, over categories that name scenarios (by name, or name + hash). An
    // entry named after no scenario, or after several, is written without a
    // hash (unresolved / ambiguous) and gets thresholds around a nominal 1000.
    struct BenchmarkTierSpec {
        std::string id;
        std::string name;
        domain::BenchmarkColor color;
        double factor = 1.0;
    };

    struct BenchmarkScenarioRef {
        std::string name;
        std::optional<std::string> hash;  // nullopt: look the name up in the profile
    };

    struct BenchmarkCategorySpec {
        std::string id;
        std::string name;
        domain::BenchmarkColor color;
        std::vector<BenchmarkScenarioRef> scenarios;
    };

    struct BenchmarkSpec {
        std::string id = "spec-benchmark";
        std::string name = "Spec Benchmark";
        std::string file;  // default: <id>.json
        std::vector<BenchmarkTierSpec> tiers;
        std::vector<BenchmarkCategorySpec> categories;
    };

    [[nodiscard]] domain::Benchmark generateBenchmark(const domain::UserProfile &profile, const BenchmarkSpec &spec);

    // A declarative profile (`ksv_driver --profile-spec FILE`; the schema is in
    // tools/driver/README.md). Scenarios are generated in the order listed, each
    // run seeded from its scenario and run index, so a spec always produces the
    // same profile.
    struct ScenarioSpec {
        std::string name;
        std::string hash;
        int runs = 10;
        int firstDay = 0;
        int daySpacing = 1;
        int runsPerDay = 1;
        int startMinute = 18 * 60;
        int minuteSpacing = 2;
        double durationSeconds = 60.0;
        bool performance = true;
        gallery::RunShape shape = gallery::shapeForHash("gallery-primary");
    };

    struct ProfileSpec {
        std::vector<ScenarioSpec> scenarios;
        std::optional<BenchmarkSpec> benchmark;
        QString benchmarkFile;  // "benchmark": "path.json" -- copied as-is (absolute)
    };

    // Parses `json` (read from `specPath`, which relative paths resolve against).
    // Returns an empty string on success, otherwise an error naming the field path.
    [[nodiscard]] QString parseProfileSpec(const QJsonObject &json, const QString &specPath, ProfileSpec &out);
    [[nodiscard]] domain::UserProfile buildProfile(const ProfileSpec &spec);

    // Reads, parses and builds `specPath` into the session's data directory:
    // profile.pb plus the spec's benchmark, if any. Returns an empty string on success.
    [[nodiscard]] QString writeProfileSpec(const QString &specPath, const QString &profilePath,
                                           const QString &benchmarksDir);
}
