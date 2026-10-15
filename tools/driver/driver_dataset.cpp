#include "driver_dataset.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>

#include "driver_spec.h"
#include "formats/protobuf/profile_serializer.h"
#include "gallery_dataset.h"

namespace ksv::driver {
    namespace {
        constexpr auto kMarkerFile = ".ksv-driver-dataset";

        struct SyntheticDataset {
            const char *name;
            gallery::Dataset dataset;
        };
        constexpr SyntheticDataset kSynthetic[]{
            {"rich", gallery::Dataset::RichProfile},
            {"single", gallery::Dataset::SingleRun},
            {"short", gallery::Dataset::ShortHistory},
            {"bunched", gallery::Dataset::BunchedHistory},
        };

        QString copyFixtures(const QString &examplesDir, const QString &nameFilter, const QString &destination) {
            const QDir source(examplesDir);
            const auto files = source.entryList({nameFilter}, QDir::Files);
            if (files.isEmpty()) return QStringLiteral("no %1 fixtures in %2").arg(nameFilter, examplesDir);
            for (const auto &file: files) {
                const QString target = QDir(destination).filePath(file);
                if (!QFile::copy(source.filePath(file), target)) {
                    return QStringLiteral("could not copy fixture %1").arg(file);
                }
                // The app edits and deletes benchmark files, so a read-only checkout must not
                // leave the session's copies read-only.
                QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner | QFileDevice::WriteUser);
            }
            return {};
        }
    }

    QString SessionData::profilePath() const { return QDir(root).filePath("profile.pb"); }
    QString SessionData::benchmarksDir() const { return QDir(root).filePath("benchmarks"); }
    QString SessionData::performancesDir() const { return QDir(root).filePath("FPSAimTrainer/performances"); }
    QString SessionData::statsDir() const { return QDir(root).filePath("FPSAimTrainer/stats"); }

    QStringList datasetNames() {
        QStringList names;
        for (const auto &synthetic: kSynthetic) names.append(QString::fromLatin1(synthetic.name));
        names << QStringLiteral("fixtures") << QStringLiteral("empty") << QStringLiteral("first-run");
        return names;
    }

    bool datasetSetsKovaaksDir(const QString &dataset) { return dataset != u"first-run"; }

    bool isSeeded(const SessionData &data) { return QFileInfo::exists(QDir(data.root).filePath(kMarkerFile)); }

    bool datasetAcceptsProfileSpec(const QString &dataset) {
        return dataset == u"empty" || dataset == u"first-run" || dataset == u"fixtures";
    }

    bool datasetShipsBenchmarks(const QString &dataset) {
        return std::ranges::any_of(kSynthetic, [&](const SyntheticDataset &synthetic) {
            return dataset == QLatin1String(synthetic.name);
        });
    }

    QString seedSessionData(const SessionData &data, const QString &dataset, const QString &examplesDir,
                            const QString &benchmarkFixturesDir, const QString &profileSpec) {
        if (!datasetNames().contains(dataset)) {
            return QStringLiteral("unknown dataset '%1' (expected one of: %2)")
                .arg(dataset, datasetNames().join(QStringLiteral(", ")));
        }
        if (!profileSpec.isEmpty() && !datasetAcceptsProfileSpec(dataset)) {
            return QStringLiteral("--profile-spec replaces the profile, so it combines only with --dataset empty, "
                                  "first-run or fixtures, not '%1'").arg(dataset);
        }
        const QString marker = QDir(data.root).filePath(kMarkerFile);
        if (isSeeded(data)) return {};  // seeded by an earlier start; keep the session's state

        for (const auto &dir: {data.root, data.benchmarksDir(), data.performancesDir(), data.statsDir()}) {
            if (!QDir().mkpath(dir)) return QStringLiteral("could not create %1").arg(dir);
        }

        for (const auto &synthetic: kSynthetic) {
            if (dataset != QLatin1String(synthetic.name)) continue;
            const auto profile = gallery::makeProfile(synthetic.dataset);
            if (!data::ProfileSerializer().save(profile, data.profilePath().toStdString())) {
                return QStringLiteral("could not write %1").arg(data.profilePath());
            }
            if (auto error = copyFixtures(benchmarkFixturesDir, "*.json", data.benchmarksDir()); !error.isEmpty()) {
                return error;
            }
        }
        if (dataset == u"fixtures") {
            if (auto error = copyFixtures(examplesDir, "*.perf", data.performancesDir()); !error.isEmpty()) return error;
            if (auto error = copyFixtures(examplesDir, "*.csv", data.statsDir()); !error.isEmpty()) return error;
        }
        if (!profileSpec.isEmpty()) {
            if (auto error = writeProfileSpec(profileSpec, data.profilePath(), data.benchmarksDir()); !error.isEmpty()) {
                return error;
            }
        }

        QFile markerFile(marker);
        if (!markerFile.open(QIODevice::WriteOnly)) return QStringLiteral("could not write %1").arg(marker);
        markerFile.write(dataset.toUtf8());
        return {};
    }
}
