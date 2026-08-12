//
// SessionViewModel tests using a hand-written fake ISessionController.
//

#include <gtest/gtest.h>

#include <QSignalSpy>

#include "session_vm.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSessionController : public ISessionController {
    public:
        std::vector<ScenarioId> scenario_list;
        int generate_call_count = 0;
        ScenarioPerf current_perf;

        std::vector<ScenarioId> getScenarioList() override { return scenario_list; }

        void generateProfileFromDirectory() override { generate_call_count++; }

        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }
        void setCurrentPerfToLatest() override {}
        void setCurrentPerf(const std::string &filename) override {}
        void setCurrentPerf(const ScenarioRunId &) override {}
        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }

        bool build_in_progress = false;
        [[nodiscard]] bool isBuildInProgress() const override { return build_in_progress; }

        [[nodiscard]] std::vector<ScenarioSummary> getScenarioSummaries() const override { return {}; }

        [[nodiscard]] std::vector<RunSummary> getRunsForScenario(const ScenarioId &) const override { return {}; }

        [[nodiscard]] std::vector<RunSummary> getRecentRuns(std::size_t) const override { return {}; }
    };

    class SessionViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> fake_controller = std::make_shared<FakeSessionController>();
    };

    TEST_F(SessionViewModelTest, GetScenarioListReturnsDisplayNamesNotHashes) {
        fake_controller->scenario_list = {ScenarioId{.name = "Long Jump", .hash = "hash-1"}};
        SessionViewModel view_model(fake_controller);

        const auto names = view_model.getScenarioList();

        ASSERT_EQ(names.size(), 1);
        EXPECT_EQ(names[0].toStdString(), "Long Jump");
    }

    TEST_F(SessionViewModelTest, UpdateScenarioHashMapDoesNotEmitWhenListUnchanged) {
        fake_controller->scenario_list = {ScenarioId{.name = "Long Jump", .hash = "hash-1"}};
        SessionViewModel view_model(fake_controller);

        const QSignalSpy spy(&view_model, &SessionViewModel::scenario_list_changed);
        view_model.updateScenarioHashMap();

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SessionViewModelTest, UpdateScenarioHashMapEmitsWhenListChanges) {
        fake_controller->scenario_list = {ScenarioId{.name = "Long Jump", .hash = "hash-1"}};
        SessionViewModel view_model(fake_controller);

        const QSignalSpy spy(&view_model, &SessionViewModel::scenario_list_changed);
        fake_controller->scenario_list.push_back(ScenarioId{.name = "Air Angelic", .hash = "hash-2"});
        view_model.updateScenarioHashMap();

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model.getScenarioList().size(), 2);
    }

    TEST_F(SessionViewModelTest, GenerateProfileDelegatesToTheController) {
        SessionViewModel view_model(fake_controller);

        view_model.generateProfile();

        EXPECT_EQ(fake_controller->generate_call_count, 1);
    }

    // The build is asynchronous now, so the scenario list can only refresh off the
    // controller's profileChanged signal, never off the generateProfile() call itself.
    TEST_F(SessionViewModelTest, ProfileChangedRefreshesScenarioList) {
        SessionViewModel view_model(fake_controller);

        fake_controller->scenario_list = {ScenarioId{.name = "Long Jump", .hash = "hash-1"}};
        emit fake_controller->profileChanged();

        EXPECT_EQ(view_model.getScenarioList().size(), 1);
    }

    TEST_F(SessionViewModelTest, BuildSignalsDriveTheProgressProperties) {
        SessionViewModel view_model(fake_controller);
        ASSERT_FALSE(view_model.profileBuildInProgress());

        const QSignalSpy spy(&view_model, &SessionViewModel::profileBuildChanged);
        emit fake_controller->buildStarted();
        EXPECT_TRUE(view_model.profileBuildInProgress());
        EXPECT_DOUBLE_EQ(view_model.profileBuildProgress(), 0.0);

        emit fake_controller->buildProgress(1, 4);
        EXPECT_DOUBLE_EQ(view_model.profileBuildProgress(), 0.25);

        emit fake_controller->buildFinished();
        EXPECT_FALSE(view_model.profileBuildInProgress());
        EXPECT_EQ(spy.count(), 3);
    }

    // App starts the profile build before it constructs the view models, so a build
    // already running has to be visible from the first binding evaluation.
    TEST_F(SessionViewModelTest, BuildAlreadyRunningAtConstructionIsReportedInProgress) {
        fake_controller->build_in_progress = true;

        const SessionViewModel view_model(fake_controller);

        EXPECT_TRUE(view_model.profileBuildInProgress());
    }

    TEST_F(SessionViewModelTest, GetCurrentPerfReturnsControllersCurrentPerf) {
        fake_controller->current_perf.run_id.scenario_id = ScenarioId{.name = "Long Jump", .hash = "hash-1"};
        SessionViewModel view_model(fake_controller);

        EXPECT_EQ(view_model.getCurrentPerf().run_id.scenario_id.name, "Long Jump");
    }

    TEST_F(SessionViewModelTest, GetCurrentPerfScenarioReturnsScenarioName) {
        fake_controller->current_perf.run_id.scenario_id = ScenarioId{.name = "Air Angelic", .hash = "hash-2"};
        SessionViewModel view_model(fake_controller);

        EXPECT_EQ(view_model.getCurrentPerfScenario(), QString("Air Angelic"));
    }
}
