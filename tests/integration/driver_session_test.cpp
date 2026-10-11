//
// Session composition for the dev-tool UI driver (tools/driver): overlays onto
// a session's data directory and typed `--setting` assignments, checked against
// what the real SettingsService reads back. Built only with the dev tools.
//

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

#include "driver_ports.h"
#include "driver_spec.h"
#include "formats/csv/stats_csv_parser.h"
#include "formats/protobuf/profile_serializer.h"
#include "formats/protobuf/proto_decoder.h"
#include "gallery_dataset.h"
#include "qt_data/benchmark_store.h"
#include "series_config_store.h"
#include "session_compose.h"
#include "settings_service.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <limits>
#include <set>

using namespace ksv;

namespace {
    bool writeFile(const QString &path, const QByteArray &content) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
    }

    QByteArray readFile(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    QJsonObject json(const char *text) { return QJsonDocument::fromJson(text).object(); }

    QString specError(const char *text) {
        driver::ProfileSpec spec;
        return driver::parseProfileSpec(json(text), "spec.json", spec);
    }

    TEST(DriverSpecTest, BuildsTheSameProfileBytesEveryTime) {
        driver::ProfileSpec spec;
        ASSERT_TRUE(driver::parseProfileSpec(json(R"({"scenarios": [
            {"name": "A", "runs": 7, "runsPerDay": 3, "daySpacing": 2},
            {"name": "B", "hash": "b-hash", "runs": 2, "shape": "gallery-pasu", "durationSeconds": 30}]})"),
                                             "spec.json", spec).isEmpty());
        QTemporaryDir dir;
        const QString specPath = QDir(dir.path()).filePath("spec.json");
        ASSERT_TRUE(writeFile(specPath, R"({"scenarios": [{"name": "A", "runs": 7, "runsPerDay": 3, "daySpacing": 2},
            {"name": "B", "hash": "b-hash", "runs": 2, "shape": "gallery-pasu", "durationSeconds": 30}]})"));
        const QString first = QDir(dir.path()).filePath("first.pb");
        const QString second = QDir(dir.path()).filePath("second.pb");
        const QString benchmarks = QDir(dir.path()).filePath("benchmarks");
        ASSERT_TRUE(driver::writeProfileSpec(specPath, first, benchmarks).isEmpty());
        ASSERT_TRUE(driver::writeProfileSpec(specPath, second, benchmarks).isEmpty());

        // The store's created_at is pinned, so no clock reaches the bytes.
        EXPECT_EQ(readFile(first), readFile(second));
        EXPECT_EQ(data::ProfileSerializer().readHeader(first.toStdString())->created_at, 1785542400);
        const auto profile = driver::buildProfile(spec);
        ASSERT_EQ(profile.getAllRunRecords().size(), 9U);
        std::set<std::string> hashes;
        for (const auto &run: profile.getAllRunRecords()) hashes.insert(run.run_id.scenario_id.hash);
        EXPECT_EQ(hashes, (std::set<std::string>{"spec-a", "b-hash"}));
    }

    TEST(DriverSpecTest, PerformanceFreeRunsKeepOnlyTheirTotals) {
        driver::ProfileSpec spec;
        ASSERT_TRUE(driver::parseProfileSpec(json(R"({"scenarios": [{"name": "Totals", "runs": 2, "performance": false}]})"),
                                             "spec.json", spec).isEmpty());
        for (const auto &run: driver::buildProfile(spec).getAllRunRecords()) {
            EXPECT_FALSE(run.performance.has_value());
            EXPECT_GT(run.stored_totals.score, 0.0F);
        }
    }

    TEST(DriverSpecTest, ErrorsNameTheOffendingField) {
        EXPECT_TRUE(specError(R"({"scenarios": [{"name": "A", "runz": 3}]})").contains("scenarios[0].runz"));
        EXPECT_TRUE(specError(R"({"scenarios": [{"runs": 3}]})").contains("scenarios[0].name: is required"));
        EXPECT_TRUE(specError(R"({"scenarios": [{"name": "A", "runsPerDay": 0}]})").contains("scenarios[0].runsPerDay"));
        EXPECT_TRUE(specError(R"({"scenarios": [{"name": "A", "shape": "wobbly"}]})").contains("unknown shape"));
        EXPECT_TRUE(specError(R"({"scenarioz": []})").contains("scenarioz"));
        EXPECT_TRUE(specError(R"({"benchmark": {"categories": [{"name": "C", "scenarios": [{"hash": "x"}]}]}})")
                        .contains("benchmark.categories[0].scenarios[0].name"));
        EXPECT_TRUE(specError(R"({"benchmark": "missing.json"})").contains("no such file"));
    }

    TEST(DriverSpecTest, GeneratedBenchmarksResolveNamesAndLeaveUnknownsUnresolved) {
        driver::ProfileSpec spec;
        ASSERT_TRUE(driver::parseProfileSpec(json(R"({"scenarios": [{"name": "Known", "runs": 2},
            {"name": "Twin", "hash": "twin-a", "runs": 1}, {"name": "Twin", "hash": "twin-b", "runs": 1}],
            "benchmark": {"name": "B", "tierFactors": [0.5, 1.0],
                          "categories": [{"name": "Cat", "scenarios": ["Known", "Ghost", "Twin",
                                                                       {"name": "Twin", "hash": "twin-b"}]}]}})"),
                                             "spec.json", spec).isEmpty());
        ASSERT_TRUE(spec.benchmark.has_value());
        const auto benchmark = driver::generateBenchmark(driver::buildProfile(spec), *spec.benchmark);

        ASSERT_EQ(benchmark.tiers.size(), 2U);
        ASSERT_EQ(benchmark.categories.size(), 1U);
        const auto &entries = benchmark.categories[0].scenarios;
        ASSERT_EQ(entries.size(), 4U);
        EXPECT_EQ(entries[0].hash, std::optional<std::string>("spec-known"));
        EXPECT_FALSE(entries[1].hash.has_value());  // no such scenario
        EXPECT_EQ(entries[1].thresholds[1].score, 1000.0);
        EXPECT_FALSE(entries[2].hash.has_value());  // two scenarios share the name
        EXPECT_EQ(entries[3].hash, std::optional<std::string>("twin-b"));
        EXPECT_NE(entries[2].id, entries[3].id);
    }

    // DAT-03: the built-in synthetic datasets keep producing the profiles they
    // always have (ksv_gallery and ksv_driver both show them).
    struct DatasetGolden {
        gallery::Dataset dataset;
        std::size_t runs;
        std::set<std::string> hashes;
        long long first;
        long long last;
        double totalScore;
    };

    TEST(DriverSpecTest, BuiltInDatasetsAreUnchanged) {
        const std::set<std::string> all{"gallery-primary", "gallery-pasu", "gallery-smoothbot", "gallery-flyts"};
        const DatasetGolden goldens[]{
            {gallery::Dataset::RichProfile, 248, all, 1785578400000, 1787940840000, 1250346.0},
            {gallery::Dataset::SingleRun, 1, {"gallery-primary"}, 1787940000000, 1787940000000, 4950.0},
            {gallery::Dataset::ShortHistory, 4, {"gallery-primary"}, 1787680800000, 1787940900000, 22290.0},
            {gallery::Dataset::BunchedHistory, 360, {"gallery-primary"}, 1785571200000, 1786042740000, 1898865.0},
        };
        for (const auto &golden: goldens) {
            const auto profile = gallery::makeProfile(golden.dataset);
            const auto &runs = profile.getAllRunRecords();
            std::set<std::string> hashes;
            long long first = std::numeric_limits<long long>::max();
            long long last = std::numeric_limits<long long>::min();
            double total = 0;
            for (const auto &run: runs) {
                hashes.insert(run.run_id.scenario_id.hash);
                first = std::min(first, run.run_id.start_time);
                last = std::max(last, run.run_id.start_time);
                total += run.stored_totals.score;
            }
            const auto name = gallery::kDatasetNames[static_cast<std::size_t>(golden.dataset)];
            EXPECT_EQ(runs.size(), golden.runs) << name;
            EXPECT_EQ(hashes, golden.hashes) << name;
            EXPECT_EQ(first, golden.first) << name;
            EXPECT_EQ(last, golden.last) << name;
            EXPECT_DOUBLE_EQ(total, golden.totalScore) << name;
        }
    }

    TEST(DriverSpecTest, TheDemoBenchmarkIsTheOneTheDatasetsShipWith) {
        const auto benchmark = driver::generateBenchmark(gallery::makeProfile(gallery::Dataset::RichProfile),
                                                         driver::demoBenchmarkSpec());
        EXPECT_EQ(benchmark.id.value, "driver-demo");
        ASSERT_EQ(benchmark.categories.size(), 2U);
        EXPECT_EQ(benchmark.categories[0].id.value, "clicking");
        EXPECT_EQ(benchmark.categories[1].scenarios[1].id.value, "entry-gallery-flyts");
        ASSERT_EQ(benchmark.tiers.size(), 5U);
        EXPECT_EQ(benchmark.tiers[4].id.value, "platinum");
    }

    TEST(DriverFaultTest, ParsesPortsOpsAndEffectsAndNamesBadInput) {
        driver::FaultPlan plan;
        EXPECT_TRUE(plan.add("benchmarkStore.write=fail").isEmpty());
        EXPECT_TRUE(plan.add("decoder=delay:5,fail:2").isEmpty());
        EXPECT_EQ(plan.describe(), (QStringList{"benchmarkStore.write=fail", "decoder=delay:5,fail:2"}));

        EXPECT_TRUE(plan.add("disk=fail").contains("unknown port 'disk'"));
        EXPECT_TRUE(plan.add("disk=fail").contains("seriesStore (createComputed"));
        EXPECT_TRUE(plan.add("csv.write=fail").contains("no op 'write'"));
        EXPECT_TRUE(plan.add("csv=explode").contains("unknown effect 'explode'"));
        EXPECT_TRUE(plan.add("csv=fail:0").contains("unknown effect"));
        EXPECT_TRUE(plan.add("csv").contains("PORT[.OP]=EFFECT"));
    }

    TEST(DriverFaultTest, CountsCallsForNthAndAfterEffects) {
        driver::FaultPlan plan;
        ASSERT_TRUE(plan.add("csv=fail:2").isEmpty());
        ASSERT_TRUE(plan.add("benchmarkStore.scan=fail:after:1").isEmpty());

        EXPECT_FALSE(plan.beforeCall("csv", "parseFile"));
        EXPECT_TRUE(plan.beforeCall("csv", "parseFile"));
        EXPECT_FALSE(plan.beforeCall("csv", "parseFile"));
        EXPECT_FALSE(plan.beforeCall("benchmarkStore", "scan"));
        EXPECT_TRUE(plan.beforeCall("benchmarkStore", "scan"));
        EXPECT_TRUE(plan.beforeCall("benchmarkStore", "scan"));
        EXPECT_FALSE(plan.beforeCall("benchmarkStore", "write"));
    }

    TEST(DriverFaultTest, DecoratorsReturnEachPortsNaturalFailure) {
        auto plan = std::make_shared<driver::FaultPlan>();
        ASSERT_TRUE(plan->add("benchmarkStore=fail").isEmpty());
        ASSERT_TRUE(plan->add("seriesStore.commitDraft=fail").isEmpty());
        ASSERT_TRUE(plan->add("decoder.decode_file=delay:60,fail").isEmpty());

        QTemporaryDir dir;
        const auto store = driver::injectBenchmarkStore(
            std::make_shared<qt_data::BenchmarkStore>(dir.path().toStdString()), plan);
        EXPECT_EQ(store->scan().failure, data::BenchmarkScanFailure::DirectoryUnavailable);
        EXPECT_EQ(store->write({}, "x.json", std::nullopt).failure, data::BenchmarkWriteFailure::WriteFailed);
        EXPECT_FALSE(store->remove("x.json", "digest").ok);
        EXPECT_EQ(store->managedDirectoryPath(), dir.path().toStdString());

        auto settings = std::make_shared<qt_data::SettingsService>(QSettings::IniFormat);
        const auto series = driver::injectSeriesStore(std::make_shared<qt_data::SeriesConfigStore>(settings), plan);
        series->beginDraft();
        EXPECT_EQ(series->commitDraft().failure, application::StoreMutationFailureCode::PersistenceWriteFailed);
        series->discardDraft();
        EXPECT_FALSE(series->getAll().empty());

        const auto decoder = driver::injectDecoder(std::make_shared<data::ProtoDecoder>(), plan);
        QElapsedTimer timer;
        timer.start();
        EXPECT_THROW(decoder->decode_file("whatever.perf"), std::runtime_error);
        EXPECT_GE(timer.elapsed(), 55);

        // Ports the plan does not name are handed through undecorated.
        const auto parser = std::make_shared<data::StatsCsvParser>();
        EXPECT_EQ(driver::injectCsvParser(parser, plan), parser);
    }


    TEST(DriverOverlayTest, CopiesTheTreeReplacingFilesAndCreatingFolders) {
        QTemporaryDir overlay;
        QTemporaryDir data;
        ASSERT_TRUE(overlay.isValid() && data.isValid());
        ASSERT_TRUE(writeFile(QDir(overlay.path()).filePath("benchmarks/broken.json"), "{"));
        ASSERT_TRUE(writeFile(QDir(overlay.path()).filePath("FPSAimTrainer/stats/a.csv"), "new"));
        ASSERT_TRUE(writeFile(QDir(data.path()).filePath("FPSAimTrainer/stats/a.csv"), "old"));

        EXPECT_TRUE(driver::applyOverlay(overlay.path(), data.path()).isEmpty());

        EXPECT_EQ(readFile(QDir(data.path()).filePath("benchmarks/broken.json")), "{");
        EXPECT_EQ(readFile(QDir(data.path()).filePath("FPSAimTrainer/stats/a.csv")), "new");
    }

    TEST(DriverOverlayTest, AMissingOverlayIsNamedInTheError) {
        QTemporaryDir data;
        const QString error = driver::applyOverlay(QDir(data.path()).filePath("no-such-overlay"), data.path());

        EXPECT_TRUE(error.contains("no-such-overlay")) << error.toStdString();
    }

    TEST(DriverSettingTest, ParsesJsonValuesStringsAndRemovals) {
        driver::SettingAssignment assignment;

        ASSERT_TRUE(driver::parseSettingAssignment("view/scenarioGraphVisible=false", assignment).isEmpty());
        EXPECT_EQ(assignment.key, "view/scenarioGraphVisible");
        EXPECT_EQ(assignment.value->typeId(), QMetaType::Bool);

        ASSERT_TRUE(driver::parseSettingAssignment("benchmarks/lastSelectedId=driver-demo", assignment).isEmpty());
        EXPECT_EQ(assignment.value->toString(), "driver-demo");

        ASSERT_TRUE(driver::parseSettingAssignment("a/list=[\"x\", \"y\"]", assignment).isEmpty());
        EXPECT_EQ(assignment.value->toStringList(), (QStringList{"x", "y"}));

        ASSERT_TRUE(driver::parseSettingAssignment("file/kovaaksDirs=", assignment).isEmpty());
        EXPECT_FALSE(assignment.value.has_value());

        EXPECT_TRUE(driver::parseSettingAssignment("no-equals", assignment).contains("no-equals"));
        EXPECT_TRUE(driver::parseSettingAssignment("=1", assignment).contains("empty key"));
    }

    TEST(DriverSettingTest, UrlKeysAreStoredTheWaySettingsServiceStoresThem) {
        qt_data::SettingsService service(QSettings::IniFormat);
        const auto apply = [&](const QString &text) {
            driver::SettingAssignment assignment;
            EXPECT_TRUE(driver::parseSettingAssignment(text, assignment).isEmpty());
            return driver::applySetting(assignment, service);
        };

        ASSERT_TRUE(apply("file/kovaaksDirs=[\"D:/Games/One\", \"E:/Games/Two\"]").isEmpty());
        EXPECT_EQ(service.getKovaaksDirs(), (std::vector<std::string>{"D:/Games/One", "E:/Games/Two"}));
        ASSERT_TRUE(apply("file/kovaaksDirs=D:/Games/Solo").isEmpty());
        EXPECT_EQ(service.getKovaaksDirs(), (std::vector<std::string>{"D:/Games/Solo"}));
        ASSERT_TRUE(apply("file/profilePath=D:/Sandbox/profile.pb").isEmpty());
        EXPECT_EQ(service.getProfilePath(), "D:/Sandbox/profile.pb");

        ASSERT_TRUE(apply("file/kovaaksDirs=").isEmpty());
        EXPECT_FALSE(service.isKovaaksDirSet());

        EXPECT_FALSE(apply("file/kovaaksDirs=42").isEmpty());
        EXPECT_FALSE(apply("file/profilePath=[1]").isEmpty());
    }

    TEST(DriverSettingTest, OtherKeysAreWrittenWithTheirJsonType) {
        qt_data::SettingsService service(QSettings::IniFormat);
        driver::SettingAssignment assignment;
        ASSERT_TRUE(driver::parseSettingAssignment("driverSettingTest/visible=false", assignment).isEmpty());
        ASSERT_TRUE(driver::applySetting(assignment, service).isEmpty());
        ASSERT_TRUE(driver::parseSettingAssignment("benchmarks/lastSelectedId=spec-bench", assignment).isEmpty());
        ASSERT_TRUE(driver::applySetting(assignment, service).isEmpty());

        const QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        EXPECT_FALSE(settings.value("driverSettingTest/visible", true).toBool());
        EXPECT_EQ(service.getLastSelectedBenchmarkId(), "spec-bench");

        ASSERT_TRUE(driver::parseSettingAssignment("benchmarks/lastSelectedId=", assignment).isEmpty());
        ASSERT_TRUE(driver::applySetting(assignment, service).isEmpty());
        EXPECT_EQ(service.getLastSelectedBenchmarkId(), "");
    }
}
