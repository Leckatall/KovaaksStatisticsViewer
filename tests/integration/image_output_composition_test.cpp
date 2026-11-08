#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/app.h"
#include "app/contracts/raster_image.h"
#include "formats/protobuf/proto_decoder.h"

#include "integration_env.h"

namespace ksv {
    namespace {
        constexpr int kWaitMs = 5000;

        application::RasterImage smallImage() {
            auto buffer = application::RasterBuffer::allocate(3, 2);
            return std::move(*buffer).freeze();
        }

        QByteArray readAll(const QString &path) {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
        }

        // An App over a real profile, loaded far enough that the profile store exists on disk.
        struct OutputFixture {
            integration::TestEnv env;
            std::unique_ptr<application::App> app;
            std::vector<application::ImageOutputResult> results;

            bool start() {
                if (!env.valid() || !env.makePerformancesDir()) return false;
                if (env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty()) return false;
                app = std::make_unique<application::App>(env.settings, std::make_shared<data::ProtoDecoder>(),
                    env.graphConfigurationPersistence, env.statsParser, env.benchmarkStore, env.daySource,
                    env.clipboardWriter, env.pngWriter);
                app->imageOutputUseCase()->onCompleted(
                    [this](const application::ImageOutputResult &result) { results.push_back(result); });
                return QTest::qWaitFor([&] { return !app->sessionController()->profileImportState().busy; }, kWaitMs)
                    && QTest::qWaitFor([&] { return QFile::exists(env.profileStorePath()); }, kWaitMs);
            }

            bool waitForResults(const std::size_t count) {
                return QTest::qWaitFor([&] { return results.size() >= count; }, kWaitMs);
            }
        };
    }

    TEST(ImageOutputComposition, AppExposesItsOutputUseCase) {
        integration::TestEnv env;
        ASSERT_TRUE(env.valid());
        application::App app(env.settings, std::make_shared<data::ProtoDecoder>(),
            env.graphConfigurationPersistence, env.statsParser, env.benchmarkStore, env.daySource,
            env.clipboardWriter, env.pngWriter);
        EXPECT_NE(app.imageOutputUseCase(), nullptr);
    }

    TEST(ImageOutputComposition, CopyReachesTheInjectedClipboardLeaf) {
        OutputFixture fixture;
        ASSERT_TRUE(fixture.start());
        const auto image = smallImage();

        const auto admission = fixture.app->imageOutputUseCase()->copyImage(image, "dashboard graph");
        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(fixture.waitForResults(1));

        const auto seen = fixture.env.clipboardWriter->seen();
        ASSERT_EQ(seen.size(), 1U);
        EXPECT_EQ(seen[0].width, image.width());
        EXPECT_EQ(seen[0].height, image.height());
        EXPECT_EQ(fixture.results[0].status, application::ImageOutputStatus::Succeeded);
        EXPECT_EQ(fixture.results[0].operationId, admission.operationId);
        EXPECT_EQ(fixture.results[0].targetLabel, "dashboard graph");
        EXPECT_EQ(fixture.env.pngWriter->writeCount(), 0U);
    }

    TEST(ImageOutputComposition, SaveReachesTheInjectedPngLeaf) {
        OutputFixture fixture;
        ASSERT_TRUE(fixture.start());
        QTemporaryDir destinationDir;
        ASSERT_TRUE(destinationDir.isValid());
        const std::filesystem::path destination = (destinationDir.path() + "/capture.png").toStdString();

        const auto admission = fixture.app->imageOutputUseCase()->savePng(smallImage(), destination, "session graph");
        ASSERT_TRUE(admission.accepted);
        ASSERT_TRUE(fixture.waitForResults(1));

        const auto seen = fixture.env.pngWriter->seen();
        ASSERT_EQ(seen.size(), 1U);
        EXPECT_EQ(seen[0].destination, destination);
        EXPECT_EQ(fixture.results[0].status, application::ImageOutputStatus::Succeeded);
        EXPECT_EQ(fixture.results[0].targetLabel, "session graph");
        // The fake writes nothing, so a file here would mean something else wrote it.
        EXPECT_FALSE(QFile::exists(destinationDir.path() + "/capture.png"));
        EXPECT_EQ(fixture.env.clipboardWriter->publishCount(), 0U);
    }

    TEST(ImageOutputComposition, OutputLeavesUserDataUntouched) {
        OutputFixture fixture;
        ASSERT_TRUE(fixture.start());
        QTemporaryDir destinationDir;
        ASSERT_TRUE(destinationDir.isValid());

        const QByteArray profileBefore = readAll(fixture.env.profileStorePath());
        const std::string graphConfigurationBefore = fixture.env.settings->getSeriesConfigDocument();
        ASSERT_FALSE(profileBefore.isEmpty());
        ASSERT_FALSE(graphConfigurationBefore.empty());

        const auto output = fixture.app->imageOutputUseCase();
        ASSERT_TRUE(output->copyImage(smallImage(), "copy").accepted);
        ASSERT_TRUE(fixture.waitForResults(1));
        ASSERT_TRUE(output->savePng(smallImage(), (destinationDir.path() + "/capture.png").toStdString(), "save").accepted);
        ASSERT_TRUE(fixture.waitForResults(2));

        EXPECT_EQ(readAll(fixture.env.profileStorePath()), profileBefore);
        EXPECT_EQ(fixture.env.settings->getSeriesConfigDocument(), graphConfigurationBefore);
    }

}
