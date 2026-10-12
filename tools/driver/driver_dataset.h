#pragma once

#include <QString>
#include <QStringList>

namespace ksv::driver {
    // Where a driver session keeps its disposable app data:
    //   <data>/FPSAimTrainer/{performances,stats}  the KovaaK's dir the app scans
    //   <data>/profile.pb                          the profile store
    //   <data>/benchmarks/                         the managed benchmark directory
    struct SessionData {
        QString root;
        [[nodiscard]] QString profilePath() const;
        [[nodiscard]] QString benchmarksDir() const;
        [[nodiscard]] QString performancesDir() const;
        [[nodiscard]] QString statsDir() const;
    };

    [[nodiscard]] QStringList datasetNames();

    // Whether an earlier start already seeded `data` (its marker file exists).
    [[nodiscard]] bool isSeeded(const SessionData &data);

    // The benchmark the synthetic datasets open on (driver-demo.json in the
    // benchmark fixtures; the driver remembers it as the last selection).
    inline constexpr auto kDefaultBenchmarkId = "driver-demo";

    // Whether seeding `dataset` copies the benchmark fixtures (the synthetic ones).
    [[nodiscard]] bool datasetShipsBenchmarks(const QString &dataset);

    // Seeds `data` for `dataset` once (a marker file records it). Returns an empty
    // string on success, otherwise what went wrong.
    //   rich|single|short|bunched  the gallery's synthetic profiles + every benchmark
    //                              in `benchmarkFixturesDir` (tools/driver/benchmarks)
    //   fixtures                   tests/examples exports, imported by the real pipeline
    //   empty                      an empty KovaaK's dir: no runs, no benchmarks
    //   first-run                  nothing configured (the first-run banner shows)
    // A `profileSpec` (driver_spec.h) then writes profile.pb and its benchmark; it
    // replaces the profile, so only empty, first-run and fixtures accept one.
    [[nodiscard]] QString seedSessionData(const SessionData &data, const QString &dataset,
                                          const QString &examplesDir, const QString &benchmarkFixturesDir,
                                          const QString &profileSpec = {});

    // Whether the dataset configures a KovaaK's directory (all but first-run).
    [[nodiscard]] bool datasetSetsKovaaksDir(const QString &dataset);
}
