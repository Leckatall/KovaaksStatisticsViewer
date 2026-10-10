#include "driver_dataset.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <map>

#include "formats/protobuf/profile_serializer.h"
#include "gallery_dataset.h"
#include "qt_data/benchmark_store.h"

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

        // The gallery profiles' scenarios (tools/gallery/gallery_dataset.cpp).
        struct DemoScenario {
            const char *name;
            const char *hash;
        };
        constexpr DemoScenario kClicking[]{{"1wall6targets Gallery", "gallery-primary"}, {"Pasu Gallery", "gallery-pasu"}};
        constexpr DemoScenario kTracking[]{{"Smoothbot Gallery", "gallery-smoothbot"},
                                           {"VT FlyTS Gallery", "gallery-flyts"}};

        // A five-tier benchmark over the gallery scenarios, with thresholds placed
        // around each scenario's personal best so the tracking view shows a mix of
        // reached and unreached tiers.
        domain::Benchmark demoBenchmark(const domain::UserProfile &profile) {
            std::map<std::string, double> best;
            for (const auto &run: profile.getAllRunRecords()) {
                auto &score = best[run.run_id.scenario_id.hash];
                score = std::max(score, static_cast<double>(run.totals().score));
            }

            struct TierShape {
                const char *id;
                const char *name;
                domain::BenchmarkColor color;
                double factor;
            };
            constexpr TierShape kTiers[]{
                {"iron", "Iron", {130, 130, 140, 255}, 0.6},
                {"bronze", "Bronze", {205, 127, 50, 255}, 0.8},
                {"silver", "Silver", {192, 192, 200, 255}, 0.95},
                {"gold", "Gold", {255, 200, 40, 255}, 1.05},
                {"platinum", "Platinum", {80, 220, 220, 255}, 1.2},
            };

            domain::Benchmark benchmark;
            benchmark.id = domain::BenchmarkId{"driver-demo"};
            benchmark.name = "Driver Demo Benchmark";
            for (const auto &tier: kTiers) benchmark.tiers.push_back({domain::TierId{tier.id}, tier.name, tier.color});

            const auto entry = [&](const DemoScenario &scenario) {
                domain::ScenarioEntry result;
                result.id = domain::ScenarioEntryId{std::string("entry-") + scenario.hash};
                result.name = scenario.name;
                result.hash = scenario.hash;
                const auto found = best.find(scenario.hash);
                const double base = found != best.end() && found->second > 0 ? found->second : 1000.0;
                for (const auto &tier: kTiers) {
                    result.thresholds.push_back({domain::TierId{tier.id}, std::round(base * tier.factor)});
                }
                return result;
            };
            domain::Category clicking{domain::GroupId{"clicking"}, "Clicking", {0, 188, 212, 255}, {}, {}};
            for (const auto &scenario: kClicking) clicking.scenarios.push_back(entry(scenario));
            domain::Category tracking{domain::GroupId{"tracking"}, "Tracking", {156, 39, 176, 255}, {}, {}};
            for (const auto &scenario: kTracking) tracking.scenarios.push_back(entry(scenario));
            benchmark.categories = {clicking, tracking};
            return benchmark;
        }

        QString copyFixtures(const QString &examplesDir, const QString &nameFilter, const QString &destination) {
            const QDir source(examplesDir);
            const auto files = source.entryList({nameFilter}, QDir::Files);
            if (files.isEmpty()) return QStringLiteral("no %1 fixtures in %2").arg(nameFilter, examplesDir);
            for (const auto &file: files) {
                if (!QFile::copy(source.filePath(file), QDir(destination).filePath(file))) {
                    return QStringLiteral("could not copy fixture %1").arg(file);
                }
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

    QString seedSessionData(const SessionData &data, const QString &dataset, const QString &examplesDir) {
        if (!datasetNames().contains(dataset)) {
            return QStringLiteral("unknown dataset '%1' (expected one of: %2)")
                .arg(dataset, datasetNames().join(QStringLiteral(", ")));
        }
        const QString marker = QDir(data.root).filePath(kMarkerFile);
        if (QFileInfo::exists(marker)) return {};  // seeded by an earlier start; keep the session's state

        for (const auto &dir: {data.root, data.benchmarksDir(), data.performancesDir(), data.statsDir()}) {
            if (!QDir().mkpath(dir)) return QStringLiteral("could not create %1").arg(dir);
        }

        for (const auto &synthetic: kSynthetic) {
            if (dataset != QLatin1String(synthetic.name)) continue;
            const auto profile = gallery::makeProfile(synthetic.dataset);
            if (!data::ProfileSerializer().save(profile, data.profilePath().toStdString())) {
                return QStringLiteral("could not write %1").arg(data.profilePath());
            }
            qt_data::BenchmarkStore store(data.benchmarksDir().toStdString());
            if (!store.write(demoBenchmark(profile), "driver-demo.json", std::nullopt).succeeded()) {
                return QStringLiteral("could not write the demo benchmark");
            }
        }
        if (dataset == u"fixtures") {
            if (auto error = copyFixtures(examplesDir, "*.perf", data.performancesDir()); !error.isEmpty()) return error;
            if (auto error = copyFixtures(examplesDir, "*.csv", data.statsDir()); !error.isEmpty()) return error;
        }

        QFile markerFile(marker);
        if (!markerFile.open(QIODevice::WriteOnly)) return QStringLiteral("could not write %1").arg(marker);
        markerFile.write(dataset.toUtf8());
        return {};
    }
}
