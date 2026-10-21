//
// SessionController tests using hand-written fakes for ISettingsService and
// IProfileService. Needs a QCoreApplication (see qt_test_main.cpp) since
// SessionController/ISessionController are QObjects with a signal.
//

#include <gtest/gtest.h>

#include <QSemaphore>
#include <QSignalSpy>
#include <QTest>
#include <filesystem>
#include <optional>
#include <unordered_map>

#include "session_controller.h"
#include "run_ingestor.h"
#include "profile_importer.h"
#include "fake_file_service.h"
#include "fake_profile_service.h"
#include "fake_settings_service.h"
#include "run_builders.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    class ResultBarrierImporter final : public ksv::data::IProfileImporter {
    public:
        explicit ResultBarrierImporter(std::shared_ptr<ksv::data::IProfileImporter> inner) : inner(std::move(inner)) {}
        std::shared_ptr<ksv::data::IProfileImporter> inner;
        mutable QSemaphore ready;
        mutable QSemaphore release;
        mutable bool first = true;
        ProfileImportResult run(const ProfileImportRequest &request, const ProfileImportProgress &progress) const override {
            auto result = inner->run(request, progress);
            if (std::exchange(first, false)) {
                ready.release();
                release.acquire();
            }
            return result;
        }
    };
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

    TEST_F(SessionControllerTest, ManualRefreshKeepsSelectedRunAndPublishesTerminalState) {
        const auto selected = makeRun("old", 100);
        fake_profile_service->latest_run = selected;
        fake_profile_service->profile_loaded = true;
        fake_profile_service->applied_profile.emplace();
        fake_profile_service->applied_profile->addRun(selected);
        fake_file_service->perfs_to_return = {makeRun("new", 200)};
        const auto controller = make_controller();
        controller->setCurrentRun(selected);
        controller->refreshProfile(ProfileImportReason::Manual);
        EXPECT_TRUE(controller->profileImportState().busy);
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_EQ(controller->getCurrentRun().run_id.start_time, 100);
        EXPECT_EQ(controller->profileImportState().summary.added, 1U);
        EXPECT_EQ(controller->profileImportState().outcome, ProfileImportOutcome::Succeeded);
    }

    TEST_F(SessionControllerTest, RefreshRequestsDuringScanCoalesceToOneFollowup) {
        fake_file_service->gate_scan = true;
        const auto controller = make_controller();
        struct ReleaseScan {
            FakeFileService &files;
            ~ReleaseScan() { files.gate_scan = false; files.scan_gate.release(8); }
        } release{*fake_file_service};
        controller->refreshProfile(ProfileImportReason::Manual);
        ASSERT_TRUE(fake_file_service->scan_entered.tryAcquire(1, 5000));
        controller->refreshProfile(ProfileImportReason::Live);
        controller->refreshProfile(ProfileImportReason::Manual);
        EXPECT_EQ(fake_profile_service->begin_build_count, 1);
        fake_file_service->gate_scan = false;
        fake_file_service->scan_gate.release();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_EQ(fake_profile_service->begin_build_count, 2);
    }

    // Gates the first scan so follow-up requests arrive while it is in flight.
    struct HeldScan {
        FakeFileService &files;
        explicit HeldScan(FakeFileService &files) : files(files) { files.gate_scan = true; }
        void release() const { files.gate_scan = false; files.scan_gate.release(8); }
        ~HeldScan() { release(); }
    };

    TEST_F(SessionControllerTest, RebuildRequestedDuringARefreshRunsNextAndSubsumesTheQueuedRefresh) {
        const HeldScan held(*fake_file_service);
        const auto controller = make_controller();
        controller->refreshProfile(ProfileImportReason::Manual);
        ASSERT_TRUE(fake_file_service->scan_entered.tryAcquire(1, 5000));
        controller->refreshProfile(ProfileImportReason::Live);
        controller->generateProfileFromDirectory();
        held.release();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_EQ(fake_profile_service->begun_modes,
                  (std::vector{ProfileImportMode::Refresh, ProfileImportMode::Rebuild}));
    }

    TEST_F(SessionControllerTest, RefreshRequestedDuringARebuildRunsAfterIt) {
        const HeldScan held(*fake_file_service);
        const auto controller = make_controller();
        controller->generateProfileFromDirectory();
        ASSERT_TRUE(fake_file_service->scan_entered.tryAcquire(1, 5000));
        controller->refreshProfile(ProfileImportReason::Live);
        held.release();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_EQ(fake_profile_service->begun_modes,
                  (std::vector{ProfileImportMode::Rebuild, ProfileImportMode::Refresh}));
    }

    TEST_F(SessionControllerTest, StaleCompletionClearsBusyOnceTheFollowupFinishes) {
        fake_profile_service->forced_outcome = ProfileImportOutcome::Stale;
        const HeldScan held(*fake_file_service);
        const auto controller = make_controller();
        controller->refreshProfile(ProfileImportReason::Manual);
        ASSERT_TRUE(fake_file_service->scan_entered.tryAcquire(1, 5000));
        controller->refreshProfile(ProfileImportReason::Live);
        held.release();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_EQ(fake_profile_service->begin_build_count, 2);
        EXPECT_EQ(controller->profileImportState().outcome, ProfileImportOutcome::Stale);
    }

    TEST_F(SessionControllerTest, SaveFailureLeavesTheControllerIdleAndReportsIt) {
        fake_profile_service->forced_outcome = ProfileImportOutcome::Failed;
        fake_profile_service->force_save_failed = true;
        fake_file_service->perfs_to_return = {makeRun("new", 200)};
        const auto controller = make_controller();
        controller->refreshProfile(ProfileImportReason::Manual);
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_FALSE(controller->isBuildInProgress());
        EXPECT_EQ(controller->profileImportState().outcome, ProfileImportOutcome::Failed);
        EXPECT_TRUE(controller->profileImportState().summary.save_failed);
    }

    TEST_F(SessionControllerTest, LiveArrivalAfterManualScanIsImportedByFollowupAndSelectsNewest) {
        fake_profile_service->profile_loaded = true;
        fake_profile_service->latest_run = makeRun("old", 100);
        fake_profile_service->applied_profile.emplace();
        fake_profile_service->applied_profile->addRun(makeRun("old", 100));
        auto barrier = std::make_shared<ResultBarrierImporter>(
            std::make_shared<ksv::data::ProfileImporter>(fake_file_service, ingestor));
        auto controller = std::make_unique<SessionController>(fake_settings_service, fake_profile_service,
            fake_file_service, std::static_pointer_cast<ksv::data::IProfileImporter>(barrier));
        struct Release {
            ResultBarrierImporter &importer;
            ~Release() { importer.release.release(); }
        } release{*barrier};
        controller->refreshProfile(ProfileImportReason::Manual);
        ASSERT_TRUE(barrier->ready.tryAcquire(1, 5000));
        fake_file_service->perfs_to_return = {makeRun("new", 200)};
        controller->refreshProfile(ProfileImportReason::Live);
        barrier->release.release();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_EQ(controller->getCurrentRun().run_id.start_time, 200);
        EXPECT_EQ(fake_profile_service->applied_profile->getAllRunRecords().size(), 2U);
        EXPECT_EQ(fake_profile_service->begin_build_count, 2);
    }

    TEST_F(SessionControllerTest, RetryableLiveFailuresStopAfterThreeAdditionalAttempts) {
        fake_file_service->scan_issues = {{"source.perf", "Incomplete file", true}};
        const auto controller = make_controller();
        QSignalSpy started(controller.get(), &ISessionController::buildStarted);
        controller->refreshProfile(ProfileImportReason::Live);
        ASSERT_TRUE(QTest::qWaitFor([&] { return started.count() == 4 && !controller->profileImportState().busy; }, 5000));
        EXPECT_FALSE(started.wait(2300));
        EXPECT_EQ(started.count(), 4);
        EXPECT_EQ(controller->profileImportState().summary.issues.size(), 1U);
    }

    TEST_F(SessionControllerTest, CsvEnrichmentRefreshesSelectedContentsWithoutChangingItsId) {
        auto old = makeRun("h", 100, 10);
        fake_profile_service->applied_profile.emplace();
        old.sources.perf = {fake_profile_service->applied_profile->ensureSource("C:/Kovaaks",
            "FPSAimTrainer/performances"), "run.perf"};
        old.performance.emplace();
        old.performance->add_data(1.0F, HITS, 3);
        fake_profile_service->applied_profile->addRun(old);
        fake_profile_service->latest_run = old;
        fake_profile_service->profile_loaded = true;
        fake_file_service->stats_files = {{"C:/Kovaaks", "FPSAimTrainer/stats", "run Stats.csv"}};
        fake_file_service->stats_by_path["run Stats.csv"] = ksv::data::ParsedStatsCsv{
            .scenario_id = {.name = "Scenario h", .hash = "h"}, .totals = {.score = 99}};
        const auto controller = make_controller();
        QSignalSpy changes(controller.get(), &ISessionController::currentRunChanged);
        controller->refreshProfile(ProfileImportReason::Manual);
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_EQ(controller->getCurrentRun().run_id, old.run_id);
        EXPECT_FLOAT_EQ(controller->getCurrentRun().stored_totals.score, 99);
        EXPECT_TRUE(controller->getCurrentRun().performance);
        EXPECT_EQ(changes.count(), 1);
    }

    TEST_F(SessionControllerTest, StartupCatchupDoesNotOverrideAUserSelectionMadeWhileItRuns) {
        const auto old = makeRun("old", 100);
        fake_profile_service->applied_profile.emplace();
        fake_profile_service->applied_profile->addRun(old);
        fake_profile_service->latest_run = old;
        fake_file_service->perfs_to_return = {makeRun("new", 200)};
        auto barrier = std::make_shared<ResultBarrierImporter>(
            std::make_shared<ksv::data::ProfileImporter>(fake_file_service, ingestor));
        auto controller = std::make_unique<SessionController>(fake_settings_service, fake_profile_service,
            fake_file_service, std::static_pointer_cast<ksv::data::IProfileImporter>(barrier));
        struct Release {
            ResultBarrierImporter &importer;
            ~Release() { importer.release.release(); }
        } release{*barrier};
        controller->refreshProfile(ProfileImportReason::Startup);
        ASSERT_TRUE(barrier->ready.tryAcquire(1, 5000));
        controller->setCurrentRun(old);
        barrier->release.release();
        ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->profileImportState().busy; }, 5000));
        EXPECT_EQ(controller->getCurrentRun().run_id.start_time, 100);
    }

    TEST_F(SessionControllerTest, GetScenarioListDelegatesToProfileService) {
        fake_profile_service->scenarios = {ScenarioId{.name = "A", .hash = "h1"}};
        const auto controller = make_controller();

        EXPECT_EQ(controller->getScenarioList().size(), 1);
    }

    TEST_F(SessionControllerTest, GenerateProfileFromDirectoryBuildsOnAWorkerAndAppliesTheResult) {
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
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
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100), makeRun("hash-2", 200)};
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
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
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
        fake_file_service->perfs_to_return = {makeRun("hash-1", 100)};
        const auto controller = make_controller();
        ASSERT_TRUE(static_cast<bool>(fake_profile_service->stored_build_requester));
        QSignalSpy spy(controller.get(), &ISessionController::profileChanged);

        // This is the hook ProfileService::loadProfile() uses when no stored profile can be loaded.
        fake_profile_service->stored_build_requester();

        ASSERT_TRUE(spy.wait(5000));
        EXPECT_EQ(fake_profile_service->apply_count, 1);
    }

    TEST_F(SessionControllerTest, ConstructorLoadsLatestPerfFromProfileService) {
        fake_profile_service->latest_run = makeRun("hash-1", 100);
        const auto controller = make_controller();

        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfEmitsSignalWhenRunIdDiffers) {
        fake_profile_service->latest_run = makeRun("hash-1", 100);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentRunChanged);
        controller->setCurrentRun(makeRun("hash-2", 200));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfDoesNotEmitSignalWhenRunIdUnchanged) {
        fake_profile_service->latest_run = makeRun("hash-1", 100);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentRunChanged);
        controller->setCurrentRun(makeRun("hash-1", 100));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByFilenameDelegatesToProfileServiceGetPerf) {
        fake_profile_service->perf_by_path["some/file.perf"] = makeRun("hash-2", 200);
        const auto controller = make_controller();

        controller->setCurrentPerf(std::string("some/file.perf"));

        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, RegistersOnProfileChangedCallbackThatRefreshesLatestPerf) {
        fake_profile_service->latest_run = makeRun("hash-1", 100);
        const auto controller = make_controller();
        ASSERT_FALSE(fake_profile_service->stored_callbacks.empty());

        fake_profile_service->latest_run = makeRun("hash-2", 200);
        fake_profile_service->notifyProfileChanged();

        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByRunIdResolvesViaProfileServiceGetRun) {
        const auto run_id = ScenarioRunId{.scenario_id = {.name = "Scenario hash-2", .hash = "hash-2"}, .start_time = 200};
        fake_profile_service->run_by_id[run_id] = makeRun("hash-2", 200);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentRunChanged);
        controller->setCurrentRun(run_id);

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByRunIdDoesNothingWhenRunNotFound) {
        fake_profile_service->latest_run = makeRun("hash-1", 100);
        const auto controller = make_controller();

        const auto unknown_run_id = ScenarioRunId{.scenario_id = {.name = "?", .hash = "unknown"}, .start_time = 999};
        const QSignalSpy spy(controller.get(), &ISessionController::currentRunChanged);
        controller->setCurrentRun(unknown_run_id);

        EXPECT_EQ(spy.count(), 0);
        EXPECT_EQ(controller->getCurrentRun().run_id.scenario_id.hash, "hash-1");
    }

}
