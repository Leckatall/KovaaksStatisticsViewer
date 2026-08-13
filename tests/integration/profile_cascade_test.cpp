//
// The profile subsystem wired for real: SettingsService -> ProfileService ->
// FileService/ProtoDecoder/ProfileSerializer. Covers the setProfilePath ->
// applyProfilePath -> loadProfile -> onProfileChanged cascade, the cache
// save/reload round-trip, and cache-version-mismatch regeneration through the
// live service (not the serializer in isolation).
//

#include <gtest/gtest.h>

#include <QDir>

#include <filesystem>
#include <fstream>
#include <memory>

#include "file_service.h"
#include "formats/protobuf/profile_serializer.h"
#include "formats/protobuf/proto_decoder.h"
#include "profile_service.h"

#include "integration_env.h"

using namespace ksv;

namespace {
    std::string readFile(const std::filesystem::path &path) {
        std::ifstream input(path, std::ios::in | std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    std::vector<std::filesystem::path> quarantineFiles(const std::filesystem::path &profilePath,
                                                        const std::string &reason) {
        std::vector<std::filesystem::path> matches;
        const auto prefix = profilePath.stem().string() + "_" + reason + "_";
        for (const auto &entry: std::filesystem::directory_iterator(profilePath.parent_path())) {
            if (entry.path().extension() == profilePath.extension() &&
                entry.path().stem().string().starts_with(prefix)) {
                matches.push_back(entry.path());
            }
        }
        return matches;
    }

    class ProfileCascadeTest : public testing::Test {
    protected:
        integration::TestEnv env;
        std::shared_ptr<data::ProtoDecoder> decoder = std::make_shared<data::ProtoDecoder>();
        std::shared_ptr<qt_data::FileService> fileService;
        std::shared_ptr<data::ProfileSerializer> serializer = std::make_shared<data::ProfileSerializer>();
        std::shared_ptr<data::ProfileService> profileService;

        void SetUp() override {
            ASSERT_TRUE(env.valid());
            ASSERT_TRUE(env.makePerformancesDir());
            ASSERT_FALSE(env.copyFixtureIntoPerformances("1wall6targets TE.perf").isEmpty());
            ASSERT_FALSE(env.copyFixtureIntoPerformances("VT FlyTS Novice S5.perf").isEmpty());

            fileService = std::make_shared<qt_data::FileService>(env.settings, decoder);
            profileService = std::make_shared<data::ProfileService>(fileService, serializer, env.settings);
        }
    };

    TEST_F(ProfileCascadeTest, ChangingProfilePathReloadsAndNotifiesObservers) {
        int notify_count = 0;
        profileService->onProfileChanged([&] { ++notify_count; });

        const QString newPath = QDir(env.dir.path()).absoluteFilePath("relocated/profile_cache.pb");
        env.settings->setProfilePath(newPath.toStdString());

        EXPECT_GE(notify_count, 1);
        EXPECT_TRUE(profileService->isProfileLoaded());
        EXPECT_FALSE(profileService->getScenarioList().empty());
    }

    TEST_F(ProfileCascadeTest, GeneratingProfileWritesCacheThatReloadsWithoutRescanning) {
        profileService->loadProfile();
        const auto originalCount = profileService->getScenarioList().size();
        ASSERT_GT(originalCount, 0u);
        ASSERT_TRUE(std::filesystem::exists(env.profileCachePath().toStdString()));

        // Wipe the source .perf files: a fresh service that still returns scenarios
        // must have loaded them from the cache, not re-scanned the (now empty) dir.
        ASSERT_TRUE(QDir(env.performancesDir()).removeRecursively());

        const auto reloadFileService = std::make_shared<qt_data::FileService>(env.settings, decoder);
        data::ProfileService reloaded(reloadFileService, std::make_shared<data::ProfileSerializer>(), env.settings);
        reloaded.loadProfile();

        EXPECT_EQ(reloaded.getScenarioList().size(), originalCount);
    }

    TEST_F(ProfileCascadeTest, MismatchedCacheVersionRegeneratesFromDirectory) {
        // Plant a cache stamped with an incompatible version at the cache path.
        cache::UserProfileCache stale;
        stale.set_version(0);
        stale.set_source_directory("bogus");
        auto *run = stale.add_runs();
        run->mutable_scenario_id()->set_name("Should Not Appear");
        run->mutable_scenario_id()->set_hash("stale-hash");
        run->set_start_time(1);
        {
            std::ofstream out(env.profileCachePath().toStdString(),
                              std::ios::out | std::ios::binary | std::ios::trunc);
            stale.SerializeToOstream(&out);
        }
        const auto profilePath = std::filesystem::path(env.profileCachePath().toStdString());
        const auto rejectedBytes = readFile(profilePath);

        profileService->loadProfile();

        EXPECT_TRUE(profileService->isProfileLoaded());
        EXPECT_TRUE(std::filesystem::exists(profilePath));
        const auto quarantined = quarantineFiles(profilePath, "version-mismatch");
        ASSERT_EQ(quarantined.size(), 1);
        EXPECT_EQ(readFile(quarantined.front()), rejectedBytes);
        EXPECT_NE(readFile(profilePath), rejectedBytes);
        const auto scenarios = profileService->getScenarioList();
        EXPECT_FALSE(scenarios.empty());
        for (const auto &s: scenarios) {
            EXPECT_NE(s.name, "Should Not Appear") << "stale cache leaked into the regenerated profile";
        }
    }
}
