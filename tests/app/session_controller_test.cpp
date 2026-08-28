//
// SessionController tests using hand-written fakes for ISettingsService and
// IProfileService. Needs a QCoreApplication (see qt_test_main.cpp) since
// SessionController/ISessionController are QObjects with a signal.
//

#include <gtest/gtest.h>

#include <QSemaphore>
#include <QSignalSpy>
#include <filesystem>
#include <optional>
#include <unordered_map>

#include "session_controller.h"
#include "run_ingestor.h"
#include "fake_file_service.h"
#include "fake_profile_service.h"
#include "fake_settings_service.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    ksv::domain::Run make_run(const std::string &hash, const long long start_time, const float score = 0.0F,
                            const int shots = 0, const int hits = 0, const float duration = 0.0F) {
        ksv::domain::Run perf;
        perf.run_id.scenario_id.name = "Scenario " + hash;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        perf.scenario_length = duration;
        perf.stored_totals = {.score = score, .shots = shots, .hits = hits, .misses = shots - hits};
        return perf;
    }

    class SessionControllerTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> fake_settings_service = [] {
            auto settings = std::make_shared<FakeSettingsService>();
            settings->dirs = {"C:/Kovaaks"};
            settings->profile_path = "C:/Profile/profile.pb";
            return settings;
        }();
        std::shared_ptr<FakeProfileService> fake_profile_service = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeFileService> fake_file_service = [] {
            auto service = std::make_shared<FakeFileService>();
            service->source_roots = {"C:/Kovaaks"};
            return service;
        }();

        std::shared_ptr<ksv::data::IRunIngestor> ingestor =
            std::make_shared<ksv::data::RunIngestor>(fake_file_service);

        std::unique_ptr<SessionController> make_controller() {
            return std::make_unique<SessionController>(fake_settings_service, fake_profile_service,
                                                        fake_file_service, ingestor);
        }
    };

    TEST_F(SessionControllerTest, GetScenarioListDelegatesToProfileService) {
        fake_profile_service->scenario_list = {ScenarioId{.name = "A", .hash = "h1"}};
        const auto controller = make_controller();

        EXPECT_EQ(controller->getScenarioList().size(), 1);
    }

    TEST_F(SessionControllerTest, GenerateProfileFromDirectoryBuildsOnAWorkerAndAppliesTheResult) {
        fake_file_service->perfs_to_return = {make_run("hash-1", 100)};
        const auto controller = make_controller();
        QSignalSpy spy(controller.get(), &ISessionController::profileChanged);

        controller->generateProfileFromDirectory();

        EXPECT_EQ(fake_profile_service->begin_build_count, 1);
        ASSERT_TRUE(spy.wait(5000));
        EXPECT_EQ(fake_profile_service->apply_count, 1);
        ASSERT_TRUE(fake_profile_service->applied_profile.has_value());
        EXPECT_EQ(fake_profile_service->applied_profile->getScenarioList().size(), 1);
    }

    TEST_F(SessionControllerTest, BuildReportsProgressAndBracketsItWithStartedAndFinished) {
        fake_file_service->perfs_to_return = {make_run("hash-1", 100), make_run("hash-2", 200)};
        const auto controller = make_controller();
        const QSignalSpy started(controller.get(), &ISessionController::buildStarted);
        QSignalSpy progress(controller.get(), &ISessionController::buildProgress);
        QSignalSpy finished(controller.get(), &ISessionController::buildFinished);

        controller->generateProfileFromDirectory();

        EXPECT_EQ(started.count(), 1);
        EXPECT_TRUE(controller->isBuildInProgress());
        ASSERT_TRUE(finished.wait(5000));
        EXPECT_FALSE(controller->isBuildInProgress());

        ASSERT_FALSE(progress.isEmpty());
        const auto last = progress.back();
        EXPECT_EQ(last.at(0).toInt(), 2);
        EXPECT_EQ(last.at(1).toInt(), 2);
    }

    TEST_F(SessionControllerTest, GenerateProfileFromDirectoryReturnsBeforeTheBuildFinishes) {
        fake_file_service->gate_scan = true;
        fake_file_service->perfs_to_return = {make_run("hash-1", 100)};
        const auto controller = make_controller();

        controller->generateProfileFromDirectory();
        ASSERT_TRUE(fake_file_service->scan_entered.tryAcquire(1, 5000));

        // The worker is parked inside the scan, so nothing can have been applied yet.
        EXPECT_EQ(fake_profile_service->apply_count, 0);

        QSignalSpy spy(controller.get(), &ISessionController::profileChanged);
        fake_file_service->scan_gate.release();
        ASSERT_TRUE(spy.wait(5000));
        EXPECT_EQ(fake_profile_service->apply_count, 1);
    }

    TEST_F(SessionControllerTest, SecondGenerateWhileOneIsInFlightDoesNotStartASecondBuild) {
        fake_file_service->gate_scan = true;
        const auto controller = make_controller();

        controller->generateProfileFromDirectory();
        ASSERT_TRUE(fake_file_service->scan_entered.tryAcquire(1, 5000));
        controller->generateProfileFromDirectory();

        EXPECT_EQ(fake_profile_service->begin_build_count, 1);

        // The queued request is honoured once the first build lands, so the second
        // scan must still happen â€” a coalesced request is deferred, never dropped.
        QSignalSpy spy(controller.get(), &ISessionController::profileChanged);
        fake_file_service->scan_gate.release();
        ASSERT_TRUE(spy.wait(5000));
        ASSERT_TRUE(fake_file_service->scan_entered.tryAcquire(1, 5000));
        fake_file_service->scan_gate.release();
        EXPECT_EQ(fake_profile_service->begin_build_count, 2);
    }

    TEST_F(SessionControllerTest, ProfileServiceBuildRequestStartsAWorkerBuild) {
        fake_file_service->perfs_to_return = {make_run("hash-1", 100)};
        const auto controller = make_controller();
        ASSERT_TRUE(static_cast<bool>(fake_profile_service->stored_build_requester));
        QSignalSpy spy(controller.get(), &ISessionController::profileChanged);

        // This is the hook ProfileService::loadProfile() uses when no stored profile can be loaded.
        fake_profile_service->stored_build_requester();

        ASSERT_TRUE(spy.wait(5000));
        EXPECT_EQ(fake_profile_service->apply_count, 1);
    }

    TEST_F(SessionControllerTest, ConstructorLoadsLatestPerfFromProfileService) {
        fake_profile_service->latest_run = make_run("hash-1", 100);
        const auto controller = make_controller();

        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfEmitsSignalWhenRunIdDiffers) {
        fake_profile_service->latest_run = make_run("hash-1", 100);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentRunChanged);
        controller->setCurrentRun(make_run("hash-2", 200));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfDoesNotEmitSignalWhenRunIdUnchanged) {
        fake_profile_service->latest_run = make_run("hash-1", 100);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentRunChanged);
        controller->setCurrentRun(make_run("hash-1", 100));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByFilenameDelegatesToProfileServiceGetPerf) {
        fake_profile_service->perf_by_path["some/file.perf"] = make_run("hash-2", 200);
        const auto controller = make_controller();

        controller->setCurrentPerf(std::string("some/file.perf"));

        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, RegistersOnProfileChangedCallbackThatRefreshesLatestPerf) {
        fake_profile_service->latest_run = make_run("hash-1", 100);
        const auto controller = make_controller();
        ASSERT_TRUE(static_cast<bool>(fake_profile_service->stored_callback));

        fake_profile_service->latest_run = make_run("hash-2", 200);
        fake_profile_service->stored_callback();

        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByRunIdResolvesViaProfileServiceGetRun) {
        const auto run_id = ScenarioRunId{.scenario_id = {.name = "Scenario hash-2", .hash = "hash-2"}, .start_time = 200};
        fake_profile_service->run_by_id[run_id] = make_run("hash-2", 200);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentRunChanged);
        controller->setCurrentRun(run_id);

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByRunIdDoesNothingWhenRunNotFound) {
        fake_profile_service->latest_run = make_run("hash-1", 100);
        const auto controller = make_controller();

        const auto unknown_run_id = ScenarioRunId{.scenario_id = {.name = "?", .hash = "unknown"}, .start_time = 999};
        const QSignalSpy spy(controller.get(), &ISessionController::currentRunChanged);
        controller->setCurrentRun(unknown_run_id);

        EXPECT_EQ(spy.count(), 0);
        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(SessionControllerTest, FakeProfileServiceGetMostRecentPerfsHonorsRequestedCount) {
        // Repairs the test-double contract: this fake previously ignored `count`
        // and always returned {}, so any test relying on it couldn't observe a
        // capped result.
        fake_profile_service->most_recent_perfs_by_hash["hash-1"] = {
            make_run("hash-1", 100), make_run("hash-1", 200), make_run("hash-1", 300)
        };

        const auto recent = fake_profile_service->getMostRecentRuns(ScenarioId{.name = "?", .hash = "hash-1"}, 2);

        ASSERT_EQ(recent.size(), 2);
        EXPECT_EQ(recent[0].run_id.start_time, 200);
        EXPECT_EQ(recent[1].run_id.start_time, 300);
    }

}
