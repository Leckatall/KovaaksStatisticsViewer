//
// ProfileService tests using a hand-written fake IFileService.
//

#include <gtest/gtest.h>

#include <unordered_map>

#include "profile_service.h"

using namespace ksv::data;
using namespace ksv::application;

namespace {
    ksv::domain::ScenarioPerf make_perf(const std::string &hash, const long long start_time,
                                        const float score = 0.0F) {
        ksv::domain::ScenarioPerf perf;
        perf.run_id.scenario_id.name = "Scenario " + hash;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.add_data(0.0F, ksv::domain::SCORE, score);
        return perf;
    }

    // setProfilePath() now creates its target file's parent directory for real, so
    // tests that repoint it must use a real (temp) path rather than a drive that may
    // not exist.
    std::filesystem::path new_profile_dir() {
        return std::filesystem::temp_directory_path() / "ksv_profile_service_test_new_dir";
    }

    std::filesystem::path new_profile_path() {
        return new_profile_dir() / "profile_cache.pb";
    }

    class FakeFileService : public IFileService {
    public:
        std::vector<ksv::domain::ScenarioPerf> perfs_to_return;
        std::unordered_map<std::string, ksv::domain::ScenarioPerf> perfs_by_path;
        ksv::domain::ScenarioPerf latest_perf;
        std::string source_directory = "fake/kovaaks/performances";
        std::function<void(const std::string &)> stored_callback;

        [[nodiscard]] std::vector<ksv::domain::ScenarioPerf> getAllPerfsFromFiles() const override {
            return perfs_to_return;
        }

        [[nodiscard]] ksv::domain::ScenarioPerf getPerfFromFile(const std::string_view filename) const override {
            return perfs_by_path.at(std::string(filename));
        }

        [[nodiscard]] ksv::domain::ScenarioPerf getLatestPerf() const override {
            return latest_perf;
        }

        [[nodiscard]] std::string getSourceDirectory() const override {
            return source_directory;
        }

        void onFilesChanged(std::function<void(const std::string &path)> callback) override {
            stored_callback = std::move(callback);
        }
    };

    // Drives ProfileService's profile-path handling: setProfilePath() stores the
    // new path and fires the change callback, exactly like the real SettingsService.
    class FakeSettingsService : public ISettingsService {
    public:
        std::string kovaaks_dir = "fake/kovaaks";
        std::string profile_path = "test_cache.pb";
        std::function<void()> profile_path_changed;

        [[nodiscard]] std::string getKovaaksDir() const override { return kovaaks_dir; }
        void setKovaaksDir(const std::string &dir) override { kovaaks_dir = dir; }
        [[nodiscard]] std::string getProfilePath() const override { return profile_path; }

        void setProfilePath(const std::string &path) override {
            profile_path = path;
            if (profile_path_changed) profile_path_changed();
        }

        void onProfilePathChanged(std::function<void()> callback) override {
            profile_path_changed = std::move(callback);
        }

        void onKovaaksDirChanged(std::function<void()>) override {}
    };

    class FakeProfileSerializer : public IProfileSerializer {
    public:
        std::optional<ksv::domain::UserProfile> profile_to_load;
        int save_count = 0;
        std::filesystem::path last_save_path;
        std::filesystem::path last_load_path;
        std::optional<ksv::domain::UserProfile> last_saved_profile;

        void save(const ksv::domain::UserProfile &profile, const std::filesystem::path &path) override {
            last_saved_profile = profile;
            last_save_path = path;
            ++save_count;
        }

        [[nodiscard]] std::optional<ksv::domain::UserProfile> load(const std::filesystem::path &path) override {
            last_load_path = path;
            return profile_to_load;
        }
    };

    class ProfileServiceTest : public testing::Test {
    protected:
        std::shared_ptr<FakeFileService> fake_file_service = std::make_shared<FakeFileService>();
        std::shared_ptr<FakeProfileSerializer> fake_serializer = std::make_shared<FakeProfileSerializer>();
        std::shared_ptr<FakeSettingsService> fake_settings = std::make_shared<FakeSettingsService>();
        ProfileService profile_service{fake_file_service, fake_serializer, fake_settings};

        void TearDown() override {
            std::filesystem::remove_all(new_profile_dir());
        }
    };

    TEST_F(ProfileServiceTest, ScenarioListEmptyBeforeProfileGenerated) {
        EXPECT_TRUE(profile_service.getScenarioList().empty());
    }

    TEST_F(ProfileServiceTest, GenerateProfileFromDirectoryUsesFileServiceSourceDirectory) {
        fake_file_service->source_directory = "D:/Kovaaks/FPSAimTrainer/performances";
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};

        profile_service.generateProfileFromDirectory();

        ASSERT_TRUE(fake_serializer->last_saved_profile.has_value());
        EXPECT_EQ(fake_serializer->last_saved_profile->getSourceDirectory(), "D:/Kovaaks/FPSAimTrainer/performances");
    }

    TEST_F(ProfileServiceTest, GenerateProfileFromDirectoryBuildsScenarioList) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100), make_perf("hash-2", 200)};

        profile_service.generateProfileFromDirectory();

        const auto scenarios = profile_service.getScenarioList();
        EXPECT_EQ(scenarios.size(), 2);
    }

    TEST_F(ProfileServiceTest, AddPerfFileToProfileAddsIncrementally) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        fake_file_service->perfs_by_path["new_run.perf"] = make_perf("hash-2", 300);
        profile_service.addPerfFileToProfile("new_run.perf");

        const auto scenarios = profile_service.getScenarioList();
        EXPECT_EQ(scenarios.size(), 2);
    }

    TEST_F(ProfileServiceTest, RegistersFilesChangedCallbackOnConstruction) {
        // ProfileService should subscribe to IFileService::onFilesChanged so new
        // files picked up by the watcher get folded into the profile automatically.
        ASSERT_TRUE(static_cast<bool>(fake_file_service->stored_callback));

        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        fake_file_service->perfs_by_path["watched.perf"] = make_perf("hash-3", 400);
        fake_file_service->stored_callback("watched.perf");

        EXPECT_EQ(profile_service.getScenarioList().size(), 2);
    }

    TEST_F(ProfileServiceTest, FilesChangedCallbackBeforeProfileLoadedDoesNotCrash) {
        ASSERT_TRUE(static_cast<bool>(fake_file_service->stored_callback));

        fake_file_service->perfs_by_path["watched.perf"] = make_perf("hash-1", 100);
        fake_file_service->stored_callback("watched.perf");

        EXPECT_FALSE(profile_service.isProfileLoaded());
        EXPECT_TRUE(profile_service.getScenarioList().empty());
    }

    TEST_F(ProfileServiceTest, GetPerfDelegatesToFileService) {
        fake_file_service->perfs_by_path["some/path.perf"] = make_perf("hash-1", 100);

        const auto perf = profile_service.getPerf("some/path.perf");

        EXPECT_EQ(perf.run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(ProfileServiceTest, GetLatestPerfIsEmptyBeforeProfileGenerated) {
        // GetLatestPerf now reads from the in-memory UserProfile rather than
        // asking IFileService directly, so it has nothing to report until the
        // profile has been generated at least once.
        fake_file_service->latest_perf = make_perf("hash-latest", 999);

        const auto perf = profile_service.getLatestPerf();

        EXPECT_TRUE(perf.run_id.scenario_id.hash.empty());
    }

    TEST_F(ProfileServiceTest, GetLatestPerfReturnsMostRecentByStartTimeAcrossScenarios) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-2", 300), make_perf("hash-1", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto perf = profile_service.getLatestPerf();

        EXPECT_EQ(perf.run_id.scenario_id.hash, "hash-2");
        EXPECT_EQ(perf.run_id.start_time, 300);
    }

    TEST_F(ProfileServiceTest, GetLatestPerfReflectsIncrementallyAddedRuns) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        fake_file_service->perfs_by_path["new_run.perf"] = make_perf("hash-2", 500);
        profile_service.addPerfFileToProfile("new_run.perf");

        const auto perf = profile_service.getLatestPerf();

        EXPECT_EQ(perf.run_id.scenario_id.hash, "hash-2");
        EXPECT_EQ(perf.run_id.start_time, 500);
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfIsNulloptBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_FALSE(profile_service.getMostRecentPerf(scenario).has_value());
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfDelegatesToProfile) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-1", 300), make_perf("hash-1", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto perf = profile_service.getMostRecentPerf(scenario);

        ASSERT_TRUE(perf.has_value());
        EXPECT_EQ(perf->run_id.start_time, 300);
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfIsNulloptForUnknownScenario) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "unknown"};
        EXPECT_FALSE(profile_service.getMostRecentPerf(scenario).has_value());
    }

    TEST_F(ProfileServiceTest, GetAverageScoreIsNulloptBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_FALSE(profile_service.getAverageScore(scenario, 2).has_value());
    }

    TEST_F(ProfileServiceTest, GetAverageScoreDelegatesToProfile) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100, 10.0F), make_perf("hash-1", 200, 20.0F), make_perf("hash-1", 300, 30.0F)
        };
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto avg = profile_service.getAverageScore(scenario, 2);

        ASSERT_TRUE(avg.has_value());
        EXPECT_FLOAT_EQ(*avg, 25.0F); // average of the 2 most recent: 20 and 30
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfsIsEmptyBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_TRUE(profile_service.getMostRecentPerfs(scenario, 2).empty());
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfsDelegatesToProfile) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-1", 300), make_perf("hash-1", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto recent = profile_service.getMostRecentPerfs(scenario, 2);

        ASSERT_EQ(recent.size(), 2);
        EXPECT_EQ(recent[0].run_id.start_time, 200);
        EXPECT_EQ(recent[1].run_id.start_time, 300);
    }

    TEST_F(ProfileServiceTest, GetRunReturnsNulloptBeforeProfileGenerated) {
        const auto run_id = ksv::domain::ScenarioRunId{.scenario_id = {.name = "?", .hash = "hash-1"}, .start_time = 100};
        EXPECT_FALSE(profile_service.getRun(run_id).has_value());
    }

    TEST_F(ProfileServiceTest, GetRunDelegatesToProfile) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100), make_perf("hash-2", 200)};
        profile_service.generateProfileFromDirectory();

        const auto run_id = ksv::domain::ScenarioRunId{.scenario_id = {.name = "?", .hash = "hash-2"}, .start_time = 200};
        const auto perf = profile_service.getRun(run_id);

        ASSERT_TRUE(perf.has_value());
        EXPECT_EQ(perf->run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(ProfileServiceTest, GetRunCountDelegatesToProfile) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100), make_perf("hash-1", 200)};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto count = profile_service.getRunCount(scenario);

        ASSERT_TRUE(count.has_value());
        EXPECT_EQ(*count, 2);
    }

    TEST_F(ProfileServiceTest, GetRunCountIsNulloptForUnknownScenario) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "unknown"};
        EXPECT_FALSE(profile_service.getRunCount(scenario).has_value());
    }

    TEST_F(ProfileServiceTest, GetLastRunTimeDelegatesToProfile) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100000), make_perf("hash-1", 300000)};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto last_played = profile_service.getLastRunTime(scenario);

        ASSERT_TRUE(last_played.has_value());
        EXPECT_EQ(*last_played, (ksv::domain::ScenarioRunId{.scenario_id = scenario, .start_time = 300000}.startSecond()));
    }

    TEST_F(ProfileServiceTest, GetLastRunTimeIsNulloptBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_FALSE(profile_service.getLastRunTime(scenario).has_value());
    }

    TEST_F(ProfileServiceTest, GetTotalTimeDelegatesToProfile) {
        ksv::domain::ScenarioPerf perf_a = make_perf("hash-1", 100);
        perf_a.scenario_length = 10.0F;
        ksv::domain::ScenarioPerf perf_b = make_perf("hash-1", 200);
        perf_b.scenario_length = 15.0F;
        fake_file_service->perfs_to_return = {perf_a, perf_b};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto total = profile_service.getTotalTime(scenario);

        ASSERT_TRUE(total.has_value());
        EXPECT_DOUBLE_EQ(*total, 25.0);
    }

    TEST_F(ProfileServiceTest, GetRecentRunsIsEmptyBeforeProfileGenerated) {
        EXPECT_TRUE(profile_service.getRecentRuns(5).empty());
    }

    TEST_F(ProfileServiceTest, GetRecentRunsReturnsNewestFirstAcrossScenarios) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-2", 300), make_perf("hash-1", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto recent = profile_service.getRecentRuns(10);

        ASSERT_EQ(recent.size(), 3);
        EXPECT_EQ(recent[0].run_id.start_time, 300);
        EXPECT_EQ(recent[1].run_id.start_time, 200);
        EXPECT_EQ(recent[2].run_id.start_time, 100);
    }

    TEST_F(ProfileServiceTest, GetRecentRunsIsCappedByCount) {
        fake_file_service->perfs_to_return = {
            make_perf("hash-1", 100), make_perf("hash-2", 300), make_perf("hash-1", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto recent = profile_service.getRecentRuns(2);

        ASSERT_EQ(recent.size(), 2);
        EXPECT_EQ(recent[0].run_id.start_time, 300);
        EXPECT_EQ(recent[1].run_id.start_time, 200);
    }

    TEST_F(ProfileServiceTest, OnProfileChangedFiresOnGenerateAndOnIncrementalAdd) {
        int notify_count = 0;
        profile_service.onProfileChanged([&notify_count] { ++notify_count; });

        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};
        profile_service.generateProfileFromDirectory();
        EXPECT_EQ(notify_count, 1);

        fake_file_service->perfs_by_path["new_run.perf"] = make_perf("hash-2", 300);
        profile_service.addPerfFileToProfile("new_run.perf");
        EXPECT_EQ(notify_count, 2);
    }

    TEST_F(ProfileServiceTest, GenerateProfileFromDirectorySavesToCache) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};

        profile_service.generateProfileFromDirectory();

        EXPECT_EQ(fake_serializer->save_count, 1);
    }

    TEST_F(ProfileServiceTest, AddPerfFileToProfileSavesToCache) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        fake_file_service->perfs_by_path["new_run.perf"] = make_perf("hash-2", 300);
        profile_service.addPerfFileToProfile("new_run.perf");

        EXPECT_EQ(fake_serializer->save_count, 2);
    }

    TEST_F(ProfileServiceTest, LoadProfileUsesCacheWhenAvailableAndSkipsDirectoryScan) {
        ksv::domain::UserProfile cached{"cached"};
        cached.addScenarioPerf(make_perf("hash-cached", 500));
        fake_serializer->profile_to_load = cached;
        // If the cache is genuinely being used instead of a directory scan,
        // this perf (only reachable via getAllPerfsFromFiles) should never appear.
        fake_file_service->perfs_to_return = {make_perf("hash-from-disk", 999)};

        profile_service.loadProfile();

        const auto scenarios = profile_service.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "hash-cached");
    }

    TEST_F(ProfileServiceTest, LoadProfileFallsBackToDirectoryScanWhenNoCache) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};

        profile_service.loadProfile();

        const auto scenarios = profile_service.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "hash-1");
        // The fresh-built profile should have been persisted for next time.
        EXPECT_EQ(fake_serializer->save_count, 1);
    }

    TEST_F(ProfileServiceTest, IsProfileLoadedFalseBeforeAnyLoadOrGenerate) {
        EXPECT_FALSE(profile_service.isProfileLoaded());
    }

    TEST_F(ProfileServiceTest, IsProfileLoadedTrueAfterGenerateProfileFromDirectory) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};

        profile_service.generateProfileFromDirectory();

        EXPECT_TRUE(profile_service.isProfileLoaded());
    }

    TEST_F(ProfileServiceTest, IsProfileLoadedTrueAfterLoadProfileFromCache) {
        ksv::domain::UserProfile cached{"cached"};
        cached.addScenarioPerf(make_perf("hash-cached", 500));
        fake_serializer->profile_to_load = cached;

        profile_service.loadProfile();

        EXPECT_TRUE(profile_service.isProfileLoaded());
    }

    TEST_F(ProfileServiceTest, ProfilePathChangeReloadsFromCacheAtNewLocation) {
        ksv::domain::UserProfile cached{"cached"};
        cached.addScenarioPerf(make_perf("hash-cached", 500));
        fake_serializer->profile_to_load = cached;

        // Changing the setting notifies ProfileService, which repoints its cache.
        fake_settings->setProfilePath(new_profile_path().string());

        const auto scenarios = profile_service.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "hash-cached");
        EXPECT_TRUE(profile_service.isProfileLoaded());
        EXPECT_EQ(fake_serializer->last_load_path, new_profile_path());
    }

    TEST_F(ProfileServiceTest, ProfilePathChangeGeneratesFreshProfileWhenNoCacheAtNewLocation) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};

        fake_settings->setProfilePath(new_profile_path().string());

        const auto scenarios = profile_service.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "hash-1");
        EXPECT_EQ(fake_serializer->save_count, 1);
        EXPECT_EQ(fake_serializer->last_save_path, new_profile_path());
    }

    TEST_F(ProfileServiceTest, ProfilePathChangeSavesToNewFileOnNextChange) {
        fake_file_service->perfs_to_return = {make_perf("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        fake_settings->setProfilePath(new_profile_path().string());

        fake_file_service->perfs_by_path["new_run.perf"] = make_perf("hash-2", 300);
        profile_service.addPerfFileToProfile("new_run.perf");

        // 1 save from the initial generate, 1 from the path-change's own fallback
        // generate (no cache at the new path), 1 from the incremental add.
        EXPECT_EQ(fake_serializer->save_count, 3);
        EXPECT_EQ(fake_serializer->last_save_path, new_profile_path());
    }
}
