//
// SessionController tests using hand-written fakes for ISettingsService and
// IProfileService. Needs a QCoreApplication (see qt_test_main.cpp) since
// SessionController/ISessionController are QObjects with a signal.
//

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <unordered_map>

#include "session_controller.h"

using namespace ksv::application;
using namespace ksv::domain;

namespace {
    class FakeSettingsService : public ISettingsService {
    public:
        std::string dir = "C:/Kovaaks";
        std::string profile_path = "C:/Profile/profile_cache.pb";

        [[nodiscard]] std::string getKovaaksDir() const override { return dir; }
        void setKovaaksDir(const std::string &new_dir) override { dir = new_dir; }
        [[nodiscard]] std::string getProfilePath() const override { return profile_path; }
        void setProfilePath(const std::string &new_path) override { profile_path = new_path; }
        void onProfilePathChanged(std::function<void()>) override {}
    };

    class FakeProfileService : public IProfileService {
    public:
        mutable int generate_call_count = 0;
        bool profile_loaded = false;
        std::vector<ScenarioId> scenario_list;
        std::unordered_map<std::string, ScenarioPerf> perf_by_path;
        ScenarioPerf latest_perf;
        std::function<void()> stored_callback;

        void generateProfileFromDirectory() override { ++generate_call_count; }

        void loadProfile() override { ++generate_call_count; }

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const override { return scenario_list; }

        [[nodiscard]] ScenarioPerf getPerf(const std::string &path) const override { return perf_by_path.at(path); }

        [[nodiscard]] ScenarioPerf getLatestPerf() const override { return latest_perf; }

        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(const ScenarioId &) const override {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<float> getAverageScore(const ScenarioId &, std::size_t) const override {
            return std::nullopt;
        }

        [[nodiscard]] bool isProfileLoaded() const override { return profile_loaded; }

        void onProfileChanged(std::function<void()> callback) override { stored_callback = std::move(callback); }
    };

    ScenarioPerf make_perf(const std::string &hash, const long long start_time) {
        ScenarioPerf perf;
        perf.run_id.scenario_id.name = "Scenario " + hash;
        perf.run_id.scenario_id.hash = hash;
        perf.run_id.start_time = start_time;
        return perf;
    }

    class SessionControllerTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> fake_settings_service = std::make_shared<FakeSettingsService>();
        std::shared_ptr<FakeProfileService> fake_profile_service = std::make_shared<FakeProfileService>();

        std::unique_ptr<SessionController> make_controller() {
            return std::make_unique<SessionController>(fake_settings_service, fake_profile_service);
        }
    };

    TEST_F(SessionControllerTest, GetScenarioListDelegatesToProfileService) {
        fake_profile_service->scenario_list = {ScenarioId{.name = "A", .hash = "h1"}};
        const auto controller = make_controller();

        EXPECT_EQ(controller->getScenarioList().size(), 1);
    }

    TEST_F(SessionControllerTest, GenerateProfileFromDirectoryDelegatesToProfileService) {
        const auto controller = make_controller();

        controller->generateProfileFromDirectory();

        EXPECT_EQ(fake_profile_service->generate_call_count, 1);
    }

    TEST_F(SessionControllerTest, ConstructorLoadsLatestPerfFromProfileService) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();

        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-1");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfEmitsSignalWhenRunIdDiffers) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentPerfChanged);
        controller->setCurrentPerf(make_perf("hash-2", 200));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, SetCurrentPerfDoesNotEmitSignalWhenRunIdUnchanged) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();

        const QSignalSpy spy(controller.get(), &ISessionController::currentPerfChanged);
        controller->setCurrentPerf(make_perf("hash-1", 100));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SessionControllerTest, SetCurrentPerfByFilenameDelegatesToProfileServiceGetPerf) {
        fake_profile_service->perf_by_path["some/file.perf"] = make_perf("hash-2", 200);
        const auto controller = make_controller();

        controller->setCurrentPerf(std::string("some/file.perf"));

        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-2");
    }

    TEST_F(SessionControllerTest, RegistersOnProfileChangedCallbackThatRefreshesLatestPerf) {
        fake_profile_service->latest_perf = make_perf("hash-1", 100);
        const auto controller = make_controller();
        ASSERT_TRUE(static_cast<bool>(fake_profile_service->stored_callback));

        fake_profile_service->latest_perf = make_perf("hash-2", 200);
        fake_profile_service->stored_callback();

        EXPECT_EQ(controller->getCurrentPerf().run_id.scenario_id.hash, "hash-2");
    }
}
