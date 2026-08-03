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

#include <tuple>

#include "file_service.h"
#include "formats/protobuf/proto_decoder.h"

using namespace ksv::qt_data;
using namespace ksv::application;

namespace {
    class FakeSettingsService : public ISettingsService {
    public:
        std::string dir;

        [[nodiscard]] std::string getKovaaksDir() const override { return dir; }
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
            settings_service->dir = temp_dir.path().toStdString();
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

    TEST_F(FileServiceTest, GetAllPerfsFromFilesReturnsEmptyWhenPerformancesDirMissing) {
        // No FPSAimTrainer/performances subdirectory created.
        const FileService file_service(settings_service, decoder);

        EXPECT_TRUE(file_service.getAllPerfsFromFiles().empty());
    }

    TEST_F(FileServiceTest, GetAllPerfsFromFilesDecodesEveryFixture) {
        makePerformancesDir();
        std::ignore = copyFixtureInto("1wall6targets TE.perf");
        std::ignore = copyFixtureInto("VT FlyTS Novice S5.perf");

        const FileService file_service(settings_service, decoder);
        const auto perfs = file_service.getAllPerfsFromFiles();

        ASSERT_EQ(perfs.size(), 2);
        bool found_known_scenario = false;
        for (const auto &perf: perfs) {
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
        std::string notified_path;
        file_service.onFilesChanged([&](const std::string &path) {
            notified = true;
            notified_path = path;
        });

        std::ignore = copyFixtureInto("1wall6targets TE.perf");

        std::ignore = QTest::qWaitFor([&] { return notified; }, 5000);

        EXPECT_TRUE(notified);
    }
}
