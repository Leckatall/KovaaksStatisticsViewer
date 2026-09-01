//
// FileService tests: exercises real filesystem access (QDir) and the
// QFileSystemWatcher-based change notification against real .perf fixtures.
//

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <set>
#include <tuple>
#include <vector>

#include "file_service.h"
#include "formats/protobuf/proto_decoder.h"
#include "data/interfaces/i_stats_csv_parser.h"
#include "fake_settings_service.h"
#include "kovaaks_dir.h"

using namespace ksv::qt_data;
using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    class RecordingStatsParser final : public ksv::data::IStatsCsvParser {
    public:
        mutable std::filesystem::path last_path;
        std::optional<ksv::data::ParsedStatsCsv> to_return;

        [[nodiscard]] std::optional<ksv::data::ParsedStatsCsv> parseFile(
            const std::filesystem::path &path) const override {
            last_path = path;
            return to_return;
        }
    };

    class FileServiceTest : public testing::Test {
    protected:
        ksv::tests_support::KovaaksDir kovaaks;
        std::shared_ptr<FakeSettingsService> settings_service = std::make_shared<FakeSettingsService>();
        std::shared_ptr<ksv::data::ProtoDecoder> decoder = std::make_shared<ksv::data::ProtoDecoder>();

        static QString fixture_path(const QString &name) {
            return QDir(TEST_FILES_DIR).absoluteFilePath(name);
        }

        void SetUp() override {
            ASSERT_TRUE(kovaaks.valid());
            settings_service->dirs = {kovaaks.root().toStdString()};
        }

        [[nodiscard]] QString performances_dir() const { return kovaaks.performancesDir(); }
        [[nodiscard]] QString stats_dir() const { return kovaaks.statsDir(); }

        void makePerformancesDir() const { ASSERT_TRUE(kovaaks.makePerformancesDir()); }
        void makeStatsDir() const { ASSERT_TRUE(kovaaks.makeStatsDir()); }

        void writeStatsFile(const QString &name) const {
            ASSERT_TRUE(kovaaks.writeIntoStats(name, "placeholder"));
        }

        [[nodiscard]] QString copyFixtureInto(const QString &fixture_name) const {
            const QString dest = kovaaks.copyIntoPerformances(fixture_path(fixture_name));
            EXPECT_FALSE(dest.isEmpty());
            return dest;
        }
    };

    TEST_F(FileServiceTest, ListPerfFilesReturnsEmptyWhenPerformancesDirMissing) {
        // No FPSAimTrainer/performances subdirectory created.
        const FileService file_service(settings_service, decoder);

        EXPECT_TRUE(file_service.listPerfFiles().empty());
    }

    TEST_F(FileServiceTest, ListPerfFilesReturnsAbsolutePathsForEveryFixture) {
        makePerformancesDir();
        std::ignore = copyFixtureInto("1wall6targets TE.perf");
        std::ignore = copyFixtureInto("VT FlyTS Novice S5.perf");

        const FileService file_service(settings_service, decoder);
        const auto paths = file_service.listPerfFiles();

        ASSERT_EQ(paths.size(), 2);
        bool found_known_scenario = false;
        for (const auto &path: paths) {
            EXPECT_TRUE(QFileInfo(QString::fromStdString(path.absolutePath())).isAbsolute());
            const auto perf = file_service.getPerfFromFile(path.absolutePath());
            if (perf.run_id.scenario_id.name == "1wall6targets TE") found_known_scenario = true;
            EXPECT_FALSE(perf.run_id.scenario_id.name.empty());
        }
        EXPECT_TRUE(found_known_scenario);
    }

    TEST_F(FileServiceTest, GetPerfFromFileDecodesGivenFile) {
        makePerformancesDir();
        const QString path = copyFixtureInto("1wall6targets TE.perf");

        const FileService file_service(settings_service, decoder);
        const auto perf = file_service.getPerfFromFile(path.toStdString());

        EXPECT_EQ(perf.run_id.scenario_id.name, "1wall6targets TE");
    }

    TEST_F(FileServiceTest, OnFilesChangedFiresWhenNewFileAppearsInPerformancesDir) {
        makePerformancesDir();
        FileService file_service(settings_service, decoder);

        bool notified = false;
        PerfFile notified_file;
        file_service.onFilesChanged([&](const PerfFile &file) {
            notified = true;
            notified_file = file;
        });

        const QString new_file = copyFixtureInto("1wall6targets TE.perf");

        std::ignore = QTest::qWaitFor([&] { return notified; }, 5000);

        ASSERT_TRUE(notified);
        EXPECT_EQ(notified_file.root, kovaaks.root().toStdString());
        EXPECT_EQ(notified_file.filename, "1wall6targets TE.perf");
        EXPECT_EQ(notified_file.absolutePath(), QDir::fromNativeSeparators(new_file).toStdString());
    }

    TEST_F(FileServiceTest, OnFilesChangedReportsEachNewFileSeparately) {
        makePerformancesDir();
        FileService file_service(settings_service, decoder);

        std::vector<PerfFile> notified_files;
        file_service.onFilesChanged([&](const PerfFile &file) {
            notified_files.push_back(file);
        });

        const QString first = copyFixtureInto("1wall6targets TE.perf");
        std::ignore = QTest::qWaitFor([&] { return !notified_files.empty(); }, 5000);

        const QString second = copyFixtureInto("VT FlyTS Novice S5.perf");
        std::ignore = QTest::qWaitFor([&] { return notified_files.size() == 2; }, 5000);

        ASSERT_EQ(notified_files.size(), 2);
        EXPECT_EQ(notified_files[0].absolutePath(), QDir::fromNativeSeparators(first).toStdString());
        EXPECT_EQ(notified_files[1].absolutePath(), QDir::fromNativeSeparators(second).toStdString());
    }

    TEST_F(FileServiceTest, FollowsKovaaksDirChangeMidSession) {
        makePerformancesDir();
        FileService file_service(settings_service, decoder);

        QTemporaryDir new_temp_dir;
        ASSERT_TRUE(new_temp_dir.isValid());
        const QString new_performances_dir = QDir(new_temp_dir.path()).absoluteFilePath("FPSAimTrainer/performances");
        ASSERT_TRUE(QDir().mkpath(new_performances_dir));
        const QString preexisting = QDir(new_performances_dir).absoluteFilePath("1wall6targets TE.perf");
        ASSERT_TRUE(QFile::copy(fixture_path("1wall6targets TE.perf"), preexisting));

        std::vector<PerfFile> notified_files;
        file_service.onFilesChanged([&](const PerfFile &file) {
            notified_files.push_back(file);
        });

        settings_service->setKovaaksDirs({new_temp_dir.path().toStdString()});

        const QString new_file = QDir(new_performances_dir).absoluteFilePath("VT FlyTS Novice S5.perf");
        ASSERT_TRUE(QFile::copy(fixture_path("VT FlyTS Novice S5.perf"), new_file));

        std::ignore = QTest::qWaitFor([&] { return !notified_files.empty(); }, 5000);

        ASSERT_EQ(notified_files.size(), 1);
        EXPECT_EQ(notified_files[0].root, new_temp_dir.path().toStdString());
        EXPECT_EQ(notified_files[0].absolutePath(), QDir::fromNativeSeparators(new_file).toStdString());
    }

    TEST_F(FileServiceTest, WatchesEveryConfiguredSourceRoot) {
        makePerformancesDir();
        QTemporaryDir second_root;
        ASSERT_TRUE(second_root.isValid());
        const auto second_performances = QDir(second_root.path()).absoluteFilePath("FPSAimTrainer/performances");
        ASSERT_TRUE(QDir().mkpath(second_performances));
        settings_service->dirs.push_back(second_root.path().toStdString());
        FileService file_service(settings_service, decoder);

        std::vector<PerfFile> notified_files;
        file_service.onFilesChanged([&](const PerfFile &file) { notified_files.push_back(file); });

        std::ignore = copyFixtureInto("1wall6targets TE.perf");
        ASSERT_TRUE(QFile::copy(fixture_path("VT FlyTS Novice S5.perf"),
                               QDir(second_performances).absoluteFilePath("VT FlyTS Novice S5.perf")));
        std::ignore = QTest::qWaitFor([&] { return notified_files.size() == 2; }, 5000);

        ASSERT_EQ(notified_files.size(), 2);
        std::set<std::string> roots;
        for (const auto &file : notified_files) roots.insert(file.root);
        EXPECT_EQ(roots, (std::set<std::string>{kovaaks.root().toStdString(), second_root.path().toStdString()}));
    }

    TEST_F(FileServiceTest, ListStatsFilesEnumeratesOnlyCsvInStatsDir) {
        makeStatsDir();
        writeStatsFile("A Stats.csv");
        writeStatsFile("B Stats.csv");
        writeStatsFile("notes.txt");

        const FileService file_service(settings_service, decoder);
        const auto stats = file_service.listStatsFiles();

        ASSERT_EQ(stats.size(), 2);
        std::set<std::string> names;
        for (const auto &entry : stats) {
            EXPECT_EQ(entry.subdir, "FPSAimTrainer/stats");
            EXPECT_EQ(entry.root, kovaaks.root().toStdString());
            EXPECT_TRUE(QFile::exists(QString::fromStdString(entry.absolutePath())));
            names.insert(entry.filename);
        }
        EXPECT_EQ(names, (std::set<std::string>{"A Stats.csv", "B Stats.csv"}));
    }

    TEST_F(FileServiceTest, GetStatsFromFileDelegatesToTheInjectedParser) {
        auto parser = std::make_shared<RecordingStatsParser>();
        parser->to_return = ksv::data::ParsedStatsCsv{.scenario_id = {.name = "S", .hash = "abc"}};
        const FileService file_service(settings_service, decoder, parser);

        const auto result = file_service.getStatsFromFile("some/dir/S Stats.csv");

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scenario_id.hash, "abc");
        EXPECT_EQ(parser->last_path, std::filesystem::path("some/dir/S Stats.csv"));
    }

    TEST_F(FileServiceTest, WatcherIgnoresFilesAddedToStatsDir) {
        makePerformancesDir();
        makeStatsDir();
        FileService file_service(settings_service, decoder);

        std::vector<PerfFile> notified_files;
        file_service.onFilesChanged([&](const PerfFile &file) { notified_files.push_back(file); });

        writeStatsFile("New Run Stats.csv");
        std::ignore = QTest::qWaitFor([&] { return !notified_files.empty(); }, 750);
        EXPECT_TRUE(notified_files.empty());

        // The watcher is alive: a real .perf arrival still fires.
        std::ignore = copyFixtureInto("1wall6targets TE.perf");
        std::ignore = QTest::qWaitFor([&] { return !notified_files.empty(); }, 5000);
        ASSERT_EQ(notified_files.size(), 1);
        EXPECT_EQ(notified_files[0].filename, "1wall6targets TE.perf");
    }
}
