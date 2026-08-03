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
        std::string kovaaks_dir = "C:/Kovaaks";
        std::vector<ScenarioId> scenario_list;
        int generate_call_count = 0;
        ScenarioPerf current_perf;

        std::vector<ScenarioId> getScenarioList() override { return scenario_list; }
        [[nodiscard]] std::string getKovaaksDir() const override { return kovaaks_dir; }

        void generateProfileFromDirectory() const override {
            const_cast<FakeSessionController *>(this)->generate_call_count++;
        }

        void setCurrentPerf(const ScenarioPerf &perf) override { current_perf = perf; }
        void setCurrentPerf(const std::string &filename) override {}
        [[nodiscard]] ScenarioPerf getCurrentPerf() const override { return current_perf; }
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

    TEST_F(SessionViewModelTest, GenerateProfileDelegatesAndRefreshesScenarioList) {
        SessionViewModel view_model(fake_controller);

        fake_controller->scenario_list = {ScenarioId{.name = "Long Jump", .hash = "hash-1"}};
        view_model.generateProfile();

        EXPECT_EQ(fake_controller->generate_call_count, 1);
        EXPECT_EQ(view_model.getScenarioList().size(), 1);
    }

    TEST_F(SessionViewModelTest, KovaaksDirReflectsControllerValueAtConstruction) {
        fake_controller->kovaaks_dir = "D:/CustomDir";
        const SessionViewModel view_model(fake_controller);

        EXPECT_EQ(view_model.getKovaaksDir(), QUrl::fromLocalFile("D:/CustomDir"));
    }

    TEST_F(SessionViewModelTest, UpdateKovaaksDirEmitsOnlyWhenDirChanges) {
        SessionViewModel view_model(fake_controller);

        const QSignalSpy spy(&view_model, &SessionViewModel::kovaaksDirChanged);

        view_model.updateKovaaksDir(); // unchanged
        EXPECT_EQ(spy.count(), 0);

        fake_controller->kovaaks_dir = "D:/NewDir";
        view_model.updateKovaaksDir();
        EXPECT_EQ(spy.count(), 1);
    }
}
