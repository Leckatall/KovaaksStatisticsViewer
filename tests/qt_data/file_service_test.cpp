//
// FileService tests: exercises real filesystem access (QDir) and the
// QFileSystemWatcher-based change notification against real .perf fixtures.
//

#include <gtest/gtest.h>

#include <QDateTime>
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

using namespace ksv::qt_data;
using namespace ksv::application;

namespace {
    class FakeSettingsService : public ISettingsService {
    public:
        std::vector<std::string> dirs;
        std::string profile_path;

        [[nodiscard]] std::vector<std::string> getKovaaksDirs() const override { return dirs; }
        [[nodiscard]] bool isKovaaksDirSet() const override { return !dirs.empty(); }
        void setKovaaksDirs(const std::vector<std::string> &new_dirs) override {
            dirs = new_dirs;
            for (const auto &callback: kovaaks_dirs_callbacks) callback();
        }
        [[nodiscard]] std::string getProfilePath() const override { return profile_path; }
        void setProfilePath(const std::string &new_path) override { profile_path = new_path; }
        void onProfilePathChanged(std::function<void()>) override {}
        void onKovaaksDirsChanged(std::function<void()> callback) override {
            kovaaks_dirs_callbacks.push_back(std::move(callback));
        }

    private:
        std::vector<std::function<void()>> kovaaks_dirs_callbacks;
    };

    class FileServiceTest : public testing::Test {
    protected:
        QTemporaryDir temp_dir;
        std::shared_ptr<FakeSettingsService> settings_service = std::make_shared<FakeSettingsService>();
        std::shared_ptr<ksv::data::ProtoDecoder> decoder = std::make_shared<ksv::data::ProtoDecoder>();

        static QString fixture_path(const QString &name) {
            return QDir(TEST_FILES_DIR).absoluteFilePath(name);
        }

        void SetUp() override {
            ASSERT_TRUE(temp_dir.isValid());
            settings_service->dirs = {temp_dir.path().toStdString()};
        }

        [[nodiscard]] QString performances_dir() const {
            return QDir(temp_dir.path()).absoluteFilePath("FPSAimTrainer/performances");
        }

        void makePerformancesDir() const {
            ASSERT_TRUE(QDir().mkpath(performances_dir()));
        }

        [[nodiscard]] QString copyFixtureInto(const QString &fixture_name) const {
            const QString dest = QDir(performances_dir()).absoluteFilePath(fixture_name);
            EXPECT_TRUE(QFile::copy(fixture_path(fixture_name), dest));
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

    TEST_F(FileServiceTest, GetLatestPerfReturnsEmptyWhenNoFiles) {
        makePerformancesDir();

        const FileService file_service(settings_service, decoder);
        const auto perf = file_service.getLatestPerf();

        EXPECT_TRUE(perf.run_id.scenario_id.name.empty());
    }

    TEST_F(FileServiceTest, GetLatestPerfReturnsMostRecentlyModifiedFile) {
        makePerformancesDir();
        const QString older = copyFixtureInto("1wall6targets TE.perf");
        const QString newer = copyFixtureInto("VT FlyTS Novice S5.perf");

        // Pin modification times explicitly so the test doesn't depend on how
        // fast the two QFile::copy calls above ran. QFile::setFileTime only
        // takes effect while the file is open.
        QFile older_file(older);
        ASSERT_TRUE(older_file.open(QIODevice::ReadWrite));
        ASSERT_TRUE(older_file.setFileTime(QDateTime::currentDateTime().addSecs(-3600), QFileDevice::FileModificationTime));
        older_file.close();

        QFile newer_file(newer);
        ASSERT_TRUE(newer_file.open(QIODevice::ReadWrite));
        ASSERT_TRUE(newer_file.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime));
        newer_file.close();

        const FileService file_service(settings_service, decoder);
        const auto perf = file_service.getLatestPerf();

        EXPECT_NE(perf.run_id.scenario_id.name, "1wall6targets TE");
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
        EXPECT_EQ(notified_file.root, temp_dir.path().toStdString());
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
        EXPECT_EQ(roots, (std::set<std::string>{temp_dir.path().toStdString(), second_root.path().toStdString()}));
    }
}
