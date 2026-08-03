//
// ProfileService tests using a hand-written fake IFileService.
//

#include <gtest/gtest.h>

#include <unordered_map>

#include "profile_service.h"

using namespace ksv::data;
using namespace ksv::application;

namespace {
    ksv::domain::ScenarioPerf make_perf(const std::string &hash, const long long start_time) {
        ksv::domain::ScenarioPerf perf;
        perf.run_id.scenario_id.name = "Scenario " + hash;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        return perf;
    }

    class FakeFileService : public IFileService {
    public:
        std::vector<ksv::domain::ScenarioPerf> perfs_to_return;
        std::unordered_map<std::string, ksv::domain::ScenarioPerf> perfs_by_path;
        ksv::domain::ScenarioPerf latest_perf;
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

        void onFilesChanged(std::function<void(const std::string &path)> callback) override {
            stored_callback = std::move(callback);
        }
    };

    class ProfileServiceTest : public testing::Test {
    protected:
        std::shared_ptr<FakeFileService> fake_file_service = std::make_shared<FakeFileService>();
        ProfileService profile_service{fake_file_service};
    };

    TEST_F(ProfileServiceTest, ScenarioListEmptyBeforeProfileGenerated) {
        EXPECT_TRUE(profile_service.getScenarioList().empty());
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

    TEST_F(ProfileServiceTest, GetPerfDelegatesToFileService) {
        fake_file_service->perfs_by_path["some/path.perf"] = make_perf("hash-1", 100);

        const auto perf = profile_service.getPerf("some/path.perf");

        EXPECT_EQ(perf.run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(ProfileServiceTest, GetLatestPerfDelegatesToFileService) {
        fake_file_service->latest_perf = make_perf("hash-latest", 999);

        const auto perf = profile_service.getLatestPerf();

        EXPECT_EQ(perf.run_id.scenario_id.hash, "hash-latest");
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
}
