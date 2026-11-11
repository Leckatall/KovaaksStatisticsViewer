#pragma once

#include <QJsonObject>
#include <QString>
#include <optional>
#include <string>
#include <vector>

#include <domain/user_profile.h>

#include "benchmarks/benchmark.h"
#include "gallery_dataset.h"
#include "modelled_run.h"

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
    struct SkillKeyframe {
        double at = 0.0;  // a run index, or a spec day when Progression::byDay
        double skill = 1.0;
    };

    // Player skill over a scenario's runs: the keyframes' linear trend (held flat
    // outside them), times a per-day and a per-run relative fluctuation.
    struct Progression {
        bool byDay = false;
        std::vector<SkillKeyframe> keyframes;
        double fluctuation = 0.0;
        double dayFluctuation = 0.0;
    };

    // Replaces the generated run at schedule index `run`. Fluctuation never applies;
    // without its own skill it takes the progression's trend at its position.
    struct HandRun {
        int run = 0;
        std::optional<double> skill;
        std::optional<gallery::PerformanceShape> shape;
    };

    // A scenario entry generated from a scenario-spec rather than a gallery shape.
    struct ModelledScenario {
        gallery::ScenarioModel model;
        gallery::PerformanceShape shape;
        double scoreScale = 1.0;
        double skill = 1.0;
        std::optional<Progression> progression;
        std::vector<HandRun> handRuns;
    };

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
        gallery::RunShape shape = gallery::shapeForHash("gallery-primary");  // unused when modelled
        std::optional<ModelledScenario> modelled;
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

    // A scenario-spec and a performance-spec file's contents (schemas in
    // tools/driver/README.md). Each returns an empty string or an error naming the field path.
    [[nodiscard]] QString parseScenarioModel(const QJsonObject &json, gallery::ScenarioModel &out);
    [[nodiscard]] QString parsePerformanceShape(const QJsonObject &json, gallery::PerformanceShape &out);

    // Reads, parses and builds `specPath` into the session's data directory:
    // profile.pb plus the spec's benchmark, if any. Returns an empty string on success.
    [[nodiscard]] QString writeProfileSpec(const QString &specPath, const QString &profilePath,
                                           const QString &benchmarksDir);
}
