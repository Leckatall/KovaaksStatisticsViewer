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

    // Seeds `data` for `dataset` once (a marker file records it). Returns an empty
    // string on success, otherwise what went wrong.
    //   rich|single|short|bunched  the gallery's synthetic profiles + a demo benchmark
    //   fixtures                   tests/examples exports, imported by the real pipeline
    //   empty                      an empty KovaaK's dir: no runs, no benchmarks
    //   first-run                  nothing configured (the first-run banner shows)
    [[nodiscard]] QString seedSessionData(const SessionData &data, const QString &dataset,
                                          const QString &examplesDir);

    // Whether the dataset configures a KovaaK's directory (all but first-run).
    [[nodiscard]] bool datasetSetsKovaaksDir(const QString &dataset);
}
