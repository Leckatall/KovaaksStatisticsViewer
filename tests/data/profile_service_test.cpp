//
// ProfileService tests using a hand-written fake IFileService.
//

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "profile_service.h"
#include "profile_importer.h"
#include "run_ingestor.h"
#include "fake_file_service.h"
#include "fake_settings_service.h"
#include "run_builders.h"

using namespace ksv::data;
using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    // setProfilePath() now creates its target file's parent directory for real, so
    // tests that repoint it must use a real (temp) path rather than a drive that may
    // not exist.
    std::filesystem::path new_profile_dir() {
        return std::filesystem::temp_directory_path() / "ksv_profile_service_test_new_dir";
    }

    std::filesystem::path new_profile_path() {
        return new_profile_dir() / "profile.pb";
    }


    class FakeProfileSerializer : public IProfileSerializer {
    public:
        std::optional<ksv::domain::UserProfile> profile_to_load;
        std::optional<ProfileStoreHeader> header_to_read;
        int save_count = 0;
        std::filesystem::path last_save_path;
        std::filesystem::path last_load_path;
        std::optional<ksv::domain::UserProfile> last_saved_profile;
        bool save_result = true;

        bool save(const ksv::domain::UserProfile &profile, const std::filesystem::path &path) override {
            last_saved_profile = profile;
            last_save_path = path;
            ++save_count;
            return save_result;
        }

        [[nodiscard]] std::optional<ProfileStoreHeader> readHeader(const std::filesystem::path &) const override {
            return header_to_read;
        }

        [[nodiscard]] ProfileLoadResult load(const std::filesystem::path &path) override {
            last_load_path = path;
            return profile_to_load
                       ? ProfileLoadResult{*profile_to_load}
                       : ProfileLoadResult{ProfileLoadError::NotFound};
        }
    };

    class ProfileServiceTest : public testing::Test {
    protected:
        std::shared_ptr<FakeFileService> fake_file_service = std::make_shared<FakeFileService>();
        std::shared_ptr<FakeProfileSerializer> fake_serializer = std::make_shared<FakeProfileSerializer>();
        std::shared_ptr<FakeSettingsService> fake_settings = [] {
            auto settings = std::make_shared<FakeSettingsService>();
            settings->dirs = {"fake/kovaaks"};
            settings->profile_path = "test_profile.pb";
            return settings;
        }();
        std::shared_ptr<IRunIngestor> ingestor = std::make_shared<RunIngestor>(fake_file_service);
        ProfileService profile_service{fake_file_service, fake_serializer, fake_settings, ingestor};

        void arrive(ksv::domain::Run run) {
            fake_file_service->perfs_to_return.push_back(std::move(run));
            fake_file_service->notifySourcesChanged();
        }

        void TearDown() override {
            std::filesystem::remove_all(new_profile_dir());
        }
    };

    TEST_F(ProfileServiceTest, ChangedBatchSavesAndNotifiesOnceWhileNoopDoesNeither) {
        profile_service.setProfile({});
        fake_file_service->perfs_to_return = {makeRun("h1", 100), makeRun("h2", 200)};
        int notifications = 0;
        profile_service.onProfileChanged([&] { ++notifications; });
        ProfileImporter importer(fake_file_service, ingestor);
        auto request = profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual);
        auto result = importer.run(request, {});
        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Succeeded);
        EXPECT_EQ(profile_service.getScenarioList().size(), 2U);
        EXPECT_EQ(fake_serializer->save_count, 1);
        EXPECT_EQ(notifications, 1);
        request = profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual);
        result = importer.run(request, {});
        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Succeeded);
        EXPECT_EQ(fake_serializer->save_count, 1);
        EXPECT_EQ(notifications, 1);
    }

    TEST_F(ProfileServiceTest, SaveFailureKeepsAcceptedHistoryAndSourcesEligibleForRetry) {
        profile_service.setProfile({});
        fake_file_service->perfs_to_return = {makeRun("new", 100)};
        ProfileImporter importer(fake_file_service, ingestor);
        auto request = profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual);
        auto result = importer.run(request, {});
        fake_serializer->save_result = false;
        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Failed);
        EXPECT_TRUE(result.summary.save_failed);
        EXPECT_TRUE(profile_service.getScenarioList().empty());
        fake_serializer->save_result = true;
        request = profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual);
        result = importer.run(request, {});
        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Succeeded);
        EXPECT_EQ(profile_service.getScenarioList().size(), 1U);
    }

    TEST_F(ProfileServiceTest, ProfilePathRoundTripInvalidatesEarlierImportEvenWhenPathMatchesAgain) {
        profile_service.setProfile({});
        fake_file_service->perfs_to_return = {makeRun("old-job", 100)};
        ProfileImporter importer(fake_file_service, ingestor);
        const auto request = profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual);
        auto result = importer.run(request, {});
        fake_serializer->profile_to_load.emplace();
        fake_settings->setProfilePath(new_profile_path().string());
        fake_settings->setProfilePath("test_profile.pb");
        const auto saves = fake_serializer->save_count;
        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Stale);
        EXPECT_EQ(fake_serializer->save_count, saves);
        EXPECT_TRUE(profile_service.getScenarioList().empty());
    }

    TEST_F(ProfileServiceTest, SourceRootRoundTripInvalidatesEarlierImport) {
        profile_service.setProfile(ksv::domain::UserProfile{});
        fake_file_service->perfs_to_return = {makeRun("old-job", 100)};
        ProfileImporter importer(fake_file_service, ingestor);
        auto result = importer.run(profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual), {});
        fake_settings->setKovaaksDirs({"C:/different"});
        fake_settings->setKovaaksDirs({"fake/kovaaks"});
        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Stale);
        EXPECT_EQ(fake_serializer->save_count, 0);
        EXPECT_TRUE(profile_service.getScenarioList().empty());
    }

    TEST_F(ProfileServiceTest, PartialResultSavesOnceAndReturnsPartial) {
        profile_service.setProfile({});
        fake_file_service->perfs_to_return = {makeRun("good", 100), makeRun("bad", 200)};
        fake_file_service->throw_for_indices.insert(1);
        ProfileImporter importer(fake_file_service, ingestor);
        auto result = importer.run(profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual), {});
        ASSERT_FALSE(result.summary.issues.empty());

        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Partial);
        EXPECT_EQ(fake_serializer->save_count, 1);
        EXPECT_EQ(profile_service.getScenarioList().size(), 1U);
    }

    TEST_F(ProfileServiceTest, AcceptingTheSameResultTwiceSavesOnceAndReturnsStale) {
        profile_service.setProfile({});
        fake_file_service->perfs_to_return = {makeRun("h", 100)};
        ProfileImporter importer(fake_file_service, ingestor);
        auto result = importer.run(profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual), {});

        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Succeeded);
        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Stale);
        EXPECT_EQ(fake_serializer->save_count, 1);
    }

    TEST_F(ProfileServiceTest, StaleResultDoesNotClearANewerActiveRequest) {
        profile_service.setProfile({});
        fake_file_service->perfs_to_return = {makeRun("h", 100)};
        ProfileImporter importer(fake_file_service, ingestor);
        const auto older = profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual);
        const auto newer = profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual);
        auto older_result = importer.run(older, {});
        auto newer_result = importer.run(newer, {});

        EXPECT_EQ(profile_service.acceptImport(older_result), ProfileImportOutcome::Stale);
        EXPECT_EQ(profile_service.acceptImport(newer_result), ProfileImportOutcome::Succeeded);
    }

    TEST_F(ProfileServiceTest, ScenarioListEmptyBeforeProfileGenerated) {
        EXPECT_TRUE(profile_service.getScenarioList().empty());
    }

    TEST_F(ProfileServiceTest, GenerateProfileFromDirectoryRegistersFileServiceRoots) {
        fake_file_service->source_roots = {"D:/Kovaaks"};
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};

        profile_service.generateProfileFromDirectory();

        ASSERT_TRUE(fake_serializer->last_saved_profile.has_value());
        const auto &entries = fake_serializer->last_saved_profile->sources().entries();
        ASSERT_EQ(entries.size(), 2);
        EXPECT_EQ(entries[0].path, "D:/Kovaaks");
    }

    TEST_F(ProfileServiceTest, GenerateProfileFromDirectoryBuildsScenarioList) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100), makeRun("hash-2", 200)};

        profile_service.generateProfileFromDirectory();

        const auto scenarios = profile_service.getScenarioList();
        EXPECT_EQ(scenarios.size(), 2);
    }

    TEST_F(ProfileServiceTest, SourcesChangedImportsAnArrivalIncrementally) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        arrive(makeRun("hash-2", 300));

        EXPECT_EQ(profile_service.getScenarioList().size(), 2);
    }

    TEST_F(ProfileServiceTest, RegistersSourcesChangedCallbackOnConstruction) {
        EXPECT_EQ(fake_file_service->callbackCount(), 1);
    }

    TEST_F(ProfileServiceTest, SourcesChangedBeforeProfileLoadedDoesNotImport) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        fake_file_service->notifySourcesChanged();

        EXPECT_FALSE(profile_service.isProfileLoaded());
        EXPECT_TRUE(profile_service.getScenarioList().empty());
    }

    TEST_F(ProfileServiceTest, GetPerfDelegatesToFileService) {
        fake_file_service->perfs_by_path["some/path.perf"] = makeRun("hash-1", 100);

        const auto perf = profile_service.getPerf("some/path.perf");

        EXPECT_EQ(perf.run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(ProfileServiceTest, GetLatestPerfIsEmptyBeforeProfileGenerated) {
        const auto perf = profile_service.getLatestRun();

        EXPECT_TRUE(perf.run_id.scenario_id.hash.empty());
    }

    TEST_F(ProfileServiceTest, GetLatestPerfReturnsMostRecentByStartTimeAcrossScenarios) {
        fake_file_service->perfs_to_return = {
            makeRun("hash-1", 100), makeRun("hash-2", 300), makeRun("hash-1", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto perf = profile_service.getLatestRun();

        EXPECT_EQ(perf.run_id.scenario_id.hash, "hash-2");
        EXPECT_EQ(perf.run_id.start_time, 300);
    }

    TEST_F(ProfileServiceTest, GetLatestPerfReflectsIncrementallyAddedRuns) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        arrive(makeRun("hash-2", 500));

        const auto perf = profile_service.getLatestRun();

        EXPECT_EQ(perf.run_id.scenario_id.hash, "hash-2");
        EXPECT_EQ(perf.run_id.start_time, 500);
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfIsNulloptBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_FALSE(profile_service.getMostRecentRun(scenario).has_value());
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfDelegatesToProfile) {
        fake_file_service->perfs_to_return = {
            makeRun("hash-1", 100), makeRun("hash-1", 300), makeRun("hash-1", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto perf = profile_service.getMostRecentRun(scenario);

        ASSERT_TRUE(perf.has_value());
        EXPECT_EQ(perf->run_id.start_time, 300);
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfIsNulloptForUnknownScenario) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "unknown"};
        EXPECT_FALSE(profile_service.getMostRecentRun(scenario).has_value());
    }

    TEST_F(ProfileServiceTest, GetAverageScoreIsNulloptBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_FALSE(profile_service.getAverageScore(scenario, 2).has_value());
    }

    TEST_F(ProfileServiceTest, GetAverageScoreDelegatesToProfile) {
        fake_file_service->perfs_to_return = {
            makeRun("hash-1", 100, 10.0F), makeRun("hash-1", 200, 20.0F), makeRun("hash-1", 300, 30.0F)
        };
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto avg = profile_service.getAverageScore(scenario, 2);

        ASSERT_TRUE(avg.has_value());
        EXPECT_FLOAT_EQ(*avg, 25.0F); // average of the 2 most recent: 20 and 30
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfsIsEmptyBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_TRUE(profile_service.getMostRecentRuns(scenario, 2).empty());
    }

    TEST_F(ProfileServiceTest, GetMostRecentPerfsDelegatesToProfile) {
        fake_file_service->perfs_to_return = {
            makeRun("hash-1", 100), makeRun("hash-1", 300), makeRun("hash-1", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto recent = profile_service.getMostRecentRuns(scenario, 2);

        ASSERT_EQ(recent.size(), 2);
        EXPECT_EQ(recent[0].run_id.start_time, 200);
        EXPECT_EQ(recent[1].run_id.start_time, 300);
    }

    TEST_F(ProfileServiceTest, GetRunReturnsNulloptBeforeProfileGenerated) {
        const auto run_id = ksv::domain::ScenarioRunId{
            .scenario_id = {.name = "?", .hash = "hash-1"}, .start_time = 100
        };
        EXPECT_FALSE(profile_service.getCurrentRun(run_id).has_value());
    }

    TEST_F(ProfileServiceTest, GetRunDelegatesToProfile) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100), makeRun("hash-2", 200)};
        profile_service.generateProfileFromDirectory();

        const auto run_id = ksv::domain::ScenarioRunId{
            .scenario_id = {.name = "?", .hash = "hash-2"}, .start_time = 200
        };
        const auto perf = profile_service.getCurrentRun(run_id);

        ASSERT_TRUE(perf.has_value());
        EXPECT_EQ(perf->run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(ProfileServiceTest, GetRunCountDelegatesToProfile) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100), makeRun("hash-1", 200)};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto count = profile_service.getRunCount(scenario);

        ASSERT_TRUE(count.has_value());
        EXPECT_EQ(*count, 2);
    }

    TEST_F(ProfileServiceTest, GetRunCountIsNulloptForUnknownScenario) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "unknown"};
        EXPECT_FALSE(profile_service.getRunCount(scenario).has_value());
    }

    TEST_F(ProfileServiceTest, GetLastRunTimeDelegatesToProfile) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100000), makeRun("hash-1", 300000)};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto last_played = profile_service.getLastRunTime(scenario);

        ASSERT_TRUE(last_played.has_value());
        EXPECT_EQ(*last_played,
                  (ksv::domain::ScenarioRunId{.scenario_id = scenario, .start_time = 300000}.startSecond()));
    }

    TEST_F(ProfileServiceTest, GetLastRunTimeIsNulloptBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_FALSE(profile_service.getLastRunTime(scenario).has_value());
    }

    TEST_F(ProfileServiceTest, GetTotalTimeDelegatesToProfile) {
        ksv::domain::Run perf_a = makeRun("hash-1", 100);
        perf_a.scenario_length = 10.0F;
        ksv::domain::Run perf_b = makeRun("hash-1", 200);
        perf_b.scenario_length = 15.0F;
        fake_file_service->perfs_to_return = {perf_a, perf_b};
        profile_service.generateProfileFromDirectory();

        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        const auto total = profile_service.getTotalTime(scenario);

        ASSERT_TRUE(total.has_value());
        EXPECT_DOUBLE_EQ(*total, 25.0);
    }

    TEST_F(ProfileServiceTest, GetRunFactsIsEmptyBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_TRUE(profile_service.getRunFacts({scenario}).empty());
    }

    TEST_F(ProfileServiceTest, GetRunFactsDelegatesToProfile) {
        ksv::domain::Run perf_a = makeRun("hash-1", 100, 10.0F);
        perf_a.scenario_length = 30.0F;
        ksv::domain::Run perf_b = makeRun("hash-2", 200, 20.0F);
        perf_b.scenario_length = 45.0F;
        fake_file_service->perfs_to_return = {perf_a, perf_b};
        profile_service.generateProfileFromDirectory();

        const auto facts = profile_service.getRunFacts({{"?", "hash-2"}});

        ASSERT_EQ(facts.size(), 1U);
        EXPECT_EQ(facts[0].run_id.scenario_id.hash, "hash-2");
        EXPECT_FLOAT_EQ(facts[0].score, 20.0F);
        EXPECT_FLOAT_EQ(facts[0].duration_seconds, 45.0F);
    }

    TEST_F(ProfileServiceTest, FilteredRollingTimeAverageIsEmptyBeforeProfileGenerated) {
        const auto scenario = ksv::domain::ScenarioId{.name = "?", .hash = "hash-1"};
        EXPECT_TRUE(profile_service.getRollingTimeAverage(
            std::vector<ksv::domain::ScenarioId>{scenario}, 3).empty());
    }

    TEST_F(ProfileServiceTest, FilteredRollingTimeAverageDelegatesToProfile) {
        constexpr long long day = 86'400'000LL;
        ksv::domain::Run perf_a = makeRun("hash-1", 100 * day);
        perf_a.scenario_length = 60.0F;
        ksv::domain::Run perf_b = makeRun("hash-2", 100 * day);
        perf_b.scenario_length = 600.0F;
        fake_file_service->perfs_to_return = {perf_a, perf_b};
        profile_service.generateProfileFromDirectory();

        const auto series = profile_service.getRollingTimeAverage(
            std::vector<ksv::domain::ScenarioId>{{"?", "hash-1"}}, 3);

        ASSERT_EQ(series.size(), 1U);
        EXPECT_DOUBLE_EQ(series[0].second, 60.0);
    }

    TEST_F(ProfileServiceTest, GetRecentRunsIsEmptyBeforeProfileGenerated) {
        EXPECT_TRUE(profile_service.getRecentRuns(5).empty());
    }

    TEST_F(ProfileServiceTest, GetRecentRunsReturnsNewestFirstAcrossScenarios) {
        fake_file_service->perfs_to_return = {
            makeRun("hash-1", 100), makeRun("hash-2", 300), makeRun("hash-1", 200)
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
            makeRun("hash-1", 100), makeRun("hash-2", 300), makeRun("hash-1", 200)
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

        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();
        EXPECT_EQ(notify_count, 1);

        arrive(makeRun("hash-2", 300));
        EXPECT_EQ(notify_count, 2);
    }

    TEST_F(ProfileServiceTest, GenerateProfileFromDirectorySavesToStore) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};

        profile_service.generateProfileFromDirectory();

        EXPECT_EQ(fake_serializer->save_count, 1);
    }

    TEST_F(ProfileServiceTest, ArrivalSavesToStore) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        arrive(makeRun("hash-2", 300));

        EXPECT_EQ(fake_serializer->save_count, 2);
    }

    TEST_F(ProfileServiceTest, ArrivalThatVanishedDuringDecodeIsSkippedAndRemembered) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();
        const auto saves_before = fake_serializer->save_count;

        fake_file_service->throw_for_indices.insert(1);
        arrive(makeRun("hash-2", 300));

        EXPECT_EQ(profile_service.getScenarioList().size(), 1);
        EXPECT_EQ(fake_serializer->save_count, saves_before + 1);
        ASSERT_TRUE(fake_serializer->last_saved_profile);
        EXPECT_EQ(fake_serializer->last_saved_profile->failedSources().size(), 1U);
    }

    TEST_F(ProfileServiceTest, FailuresOnlyResultSavesWithoutNotifyingProfileChanged) {
        profile_service.setProfile({});
        fake_file_service->perfs_to_return = {makeRun("bad", 100)};
        fake_file_service->throw_for_indices.insert(0);
        int notifications = 0;
        profile_service.onProfileChanged([&] { ++notifications; });
        ProfileImporter importer(fake_file_service, ingestor);
        auto result = importer.run(profile_service.beginImport(ProfileImportMode::Refresh, ProfileImportReason::Manual), {});
        ASSERT_FALSE(result.changed);
        ASSERT_TRUE(result.failures_changed);

        EXPECT_EQ(profile_service.acceptImport(result), ProfileImportOutcome::Partial);
        EXPECT_EQ(fake_serializer->save_count, 1);
        EXPECT_EQ(notifications, 0);
        ASSERT_TRUE(fake_serializer->last_saved_profile);
        EXPECT_EQ(fake_serializer->last_saved_profile->failedSources().size(), 1U);
    }

    TEST_F(ProfileServiceTest, BeginImportSkipsUnchangedFailuresOnlyForAutomaticRefreshes) {
        profile_service.setProfile({});
        const auto skips = [&](ProfileImportMode mode, ProfileImportReason reason) {
            return profile_service.beginImport(mode, reason).skip_unchanged_failures;
        };
        EXPECT_TRUE(skips(ProfileImportMode::Refresh, ProfileImportReason::Startup));
        EXPECT_TRUE(skips(ProfileImportMode::Refresh, ProfileImportReason::Live));
        EXPECT_TRUE(skips(ProfileImportMode::Refresh, ProfileImportReason::SourceSettingsChanged));
        EXPECT_FALSE(skips(ProfileImportMode::Refresh, ProfileImportReason::Manual));
        EXPECT_FALSE(skips(ProfileImportMode::Refresh, ProfileImportReason::Retry));
        EXPECT_FALSE(skips(ProfileImportMode::Rebuild, ProfileImportReason::Startup));
        EXPECT_FALSE(skips(ProfileImportMode::Rebuild, ProfileImportReason::Manual));
    }


    TEST_F(ProfileServiceTest, LoadProfileUsesStoreWhenAvailableAndSkipsDirectoryScan) {
        ksv::domain::UserProfile stored;
        stored.addRun(makeRun("hash-stored", 500));
        fake_serializer->profile_to_load = stored;
        // If the store is genuinely being used instead of a directory scan,
        // this perf (only reachable via getAllPerfsFromFiles) should never appear.
        fake_file_service->perfs_to_return = {makeRun("hash-from-disk", 999)};

        profile_service.loadProfile();

        const auto scenarios = profile_service.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "hash-stored");
    }

    // Loading the store is not a generate: rewriting the file we just read back to disk
    // would turn every startup into a write.
    TEST_F(ProfileServiceTest, LoadProfileFromStoreDoesNotSave) {
        ksv::domain::UserProfile stored;
        stored.addRun(makeRun("hash-stored", 500));
        fake_serializer->profile_to_load = stored;

        profile_service.loadProfile();

        EXPECT_EQ(fake_serializer->save_count, 0);
    }

    TEST_F(ProfileServiceTest, LoadProfileFallsBackToDirectoryScanWhenNoStore) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};

        profile_service.loadProfile();

        const auto scenarios = profile_service.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "hash-1");
        // The fresh-built profile should have been persisted for next time.
        EXPECT_EQ(fake_serializer->save_count, 1);
    }

    TEST_F(ProfileServiceTest, LoadProfileDelegatesToTheBuildRequesterWhenOneIsInstalled) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        int request_count = 0;
        profile_service.onBuildRequested([&request_count] { ++request_count; });

        profile_service.loadProfile();

        EXPECT_EQ(request_count, 1);
        // The requester owns the build now; nothing may have been built here.
        EXPECT_FALSE(profile_service.isProfileLoaded());
        EXPECT_EQ(fake_serializer->save_count, 0);
    }

    TEST_F(ProfileServiceTest, ArrivalIsImportedOnceWithOneSaveAndOneNotification) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();
        int notifications = 0;
        profile_service.onProfileChanged([&] { ++notifications; });

        arrive(makeRun("hash-2", 300));

        EXPECT_EQ(profile_service.getScenarioList().size(), 2);
        EXPECT_EQ(fake_serializer->save_count, 2);
        EXPECT_EQ(notifications, 1);
    }

    // A refresh that completes with nothing new must not rewrite the store.
    TEST_F(ProfileServiceTest, RepeatedSourcesChangedWithNothingNewDoesNotSaveAgain) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        fake_file_service->notifySourcesChanged();
        fake_file_service->notifySourcesChanged();

        EXPECT_EQ(fake_serializer->save_count, 1);
    }

    TEST_F(ProfileServiceTest, IsProfileLoadedFalseBeforeAnyLoadOrGenerate) {
        EXPECT_FALSE(profile_service.isProfileLoaded());
    }

    TEST_F(ProfileServiceTest, IsProfileLoadedTrueAfterGenerateProfileFromDirectory) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};

        profile_service.generateProfileFromDirectory();

        EXPECT_TRUE(profile_service.isProfileLoaded());
    }

    TEST_F(ProfileServiceTest, IsProfileLoadedTrueAfterLoadProfileFromStore) {
        ksv::domain::UserProfile stored;
        stored.addRun(makeRun("hash-stored", 500));
        fake_serializer->profile_to_load = stored;

        profile_service.loadProfile();

        EXPECT_TRUE(profile_service.isProfileLoaded());
    }

    TEST_F(ProfileServiceTest, ProfilePathChangeReloadsFromStoreAtNewLocation) {
        ksv::domain::UserProfile stored;
        stored.addRun(makeRun("hash-stored", 500));
        fake_serializer->profile_to_load = stored;

        // Changing the setting notifies ProfileService, which repoints its store.
        fake_settings->setProfilePath(new_profile_path().string());

        const auto scenarios = profile_service.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "hash-stored");
        EXPECT_TRUE(profile_service.isProfileLoaded());
        EXPECT_EQ(fake_serializer->last_load_path, new_profile_path());
    }

    TEST_F(ProfileServiceTest, ProfilePathChangeGeneratesFreshProfileWhenNoStoreAtNewLocation) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};

        fake_settings->setProfilePath(new_profile_path().string());

        const auto scenarios = profile_service.getScenarioList();
        ASSERT_EQ(scenarios.size(), 1);
        EXPECT_EQ(scenarios[0].hash, "hash-1");
        EXPECT_EQ(fake_serializer->save_count, 1);
        EXPECT_EQ(fake_serializer->last_save_path, new_profile_path());
    }

    TEST_F(ProfileServiceTest, ProfilePathChangeSavesToNewFileOnNextChange) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        profile_service.generateProfileFromDirectory();

        fake_settings->setProfilePath(new_profile_path().string());

        arrive(makeRun("hash-2", 300));

        // 1 save from the initial generate, 1 from the path-change's own fallback
        // generate (no store at the new path), 1 from the incremental add.
        EXPECT_EQ(fake_serializer->save_count, 3);
        EXPECT_EQ(fake_serializer->last_save_path, new_profile_path());
    }

    TEST_F(ProfileServiceTest, DelegatesScenarioRunsInCompletedOrder) {
        fake_file_service->perfs_to_return = {
            makeRun("hash-1", 300), makeRun("hash-1", 100), makeRun("hash-2", 200)
        };
        profile_service.generateProfileFromDirectory();

        const auto runs = profile_service.getRunsForScenario({.name = "?", .hash = "hash-1"});
        ASSERT_EQ(runs.size(), 2U);
        EXPECT_EQ(runs[0].run_id.start_time, 100);
        EXPECT_EQ(runs[1].run_id.start_time, 300);

        ProfileService unloaded(fake_file_service, fake_serializer, fake_settings, ingestor);
        EXPECT_TRUE(unloaded.getRunsForScenario({.name = "?", .hash = "hash-1"}).empty());
    }
}
