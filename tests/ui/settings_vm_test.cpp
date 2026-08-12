//
// SettingsViewModel tests using a hand-written fake ISettingsService.
//

#include <gtest/gtest.h>

#include <QSignalSpy>

#include "settings_vm.h"

using namespace ksv::presentation;
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
        void onKovaaksDirChanged(std::function<void()>) override {}
    };

    class FakeProfileService : public IProfileService {
    public:
        bool profile_loaded = false;
        std::function<void()> stored_callback;

        void generateProfileFromDirectory() override {
        }

        void loadProfile() override {
        }

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const override { return {}; }
        [[nodiscard]] ScenarioPerf getPerf(const std::string &) const override { return {}; }
        [[nodiscard]] ScenarioPerf getLatestPerf() const override { return {}; }

        [[nodiscard]] std::optional<ScenarioPerf> getMostRecentPerf(
            const ScenarioId &) const override { return std::nullopt; }

        [[nodiscard]] std::vector<ScenarioPerf> getMostRecentPerfs(
            const ScenarioId &, std::size_t) const override { return {}; }

        [[nodiscard]] std::optional<float> getAverageScore(
            const ScenarioId &, std::size_t) const override { return std::nullopt; }

        [[nodiscard]] std::optional<ScenarioPerf> getRun(const ScenarioRunId &) const override { return std::nullopt; }

        [[nodiscard]] std::optional<std::chrono::sys_seconds> getLastRunTime(const ScenarioId &) const override { return std::nullopt; }
        [[nodiscard]] std::optional<std::size_t> getRunCount(const ScenarioId &) const override { return std::nullopt; }

        [[nodiscard]] std::optional<double> getTotalTime(const ScenarioId &) const override { return std::nullopt; }

        [[nodiscard]] std::vector<ScenarioPerf> getRecentRuns(std::size_t) const override { return {}; }

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double>>
        getRollingTimeAverage(int) const override { return {}; }

        [[nodiscard]] bool isProfileLoaded() const override { return profile_loaded; }

        void onProfileChanged(std::function<void()> callback) override { stored_callback = std::move(callback); }
    };

    class SettingsViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> fake_service = std::make_shared<FakeSettingsService>();
        std::shared_ptr<FakeProfileService> fake_profile_service = std::make_shared<FakeProfileService>();

        std::unique_ptr<SettingsViewModel> make_view_model() {
            return std::make_unique<SettingsViewModel>(fake_service, fake_profile_service);
        }
    };

    TEST_F(SettingsViewModelTest, KovaaksDirReflectsServiceValueAtConstruction) {
        fake_service->dir = "D:/CustomDir";
        const auto view_model = make_view_model();

        EXPECT_EQ(view_model->getKovaaksDir(), QUrl::fromLocalFile("D:/CustomDir"));
    }

    TEST_F(SettingsViewModelTest, SetKovaaksDirUpdatesServiceAndEmitsOnChange) {
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::kovaaksDirChanged);
        view_model->setKovaaksDir(QUrl::fromLocalFile("D:/NewDir"));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->getKovaaksDir(), QUrl::fromLocalFile("D:/NewDir"));
        EXPECT_EQ(fake_service->dir, "D:/NewDir");
    }

    TEST_F(SettingsViewModelTest, SetKovaaksDirDoesNotEmitWhenUnchanged) {
        fake_service->dir = "C:/Kovaaks";
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::kovaaksDirChanged);
        view_model->setKovaaksDir(QUrl::fromLocalFile("C:/Kovaaks"));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SettingsViewModelTest, ProfilePathReflectsServiceValueAtConstruction) {
        fake_service->profile_path = "D:/CustomProfile/profile_cache.pb";
        const auto view_model = make_view_model();

        EXPECT_EQ(view_model->getProfilePath(), QUrl::fromLocalFile("D:/CustomProfile/profile_cache.pb"));
    }

    TEST_F(SettingsViewModelTest, SetProfilePathUpdatesSettingsServiceAndEmitsOnChange) {
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profilePathChanged);
        view_model->setProfilePath(QUrl::fromLocalFile("D:/NewProfile/profile_cache.pb"));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->getProfilePath(), QUrl::fromLocalFile("D:/NewProfile/profile_cache.pb"));
        EXPECT_EQ(fake_service->profile_path, "D:/NewProfile/profile_cache.pb");
    }

    TEST_F(SettingsViewModelTest, SetProfilePathDoesNotEmitWhenUnchanged) {
        fake_service->profile_path = "C:/Profile/profile_cache.pb";
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profilePathChanged);
        view_model->setProfilePath(QUrl::fromLocalFile("C:/Profile/profile_cache.pb"));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SettingsViewModelTest, ProfileLoadedReflectsProfileServiceState) {
        fake_profile_service->profile_loaded = true;
        const auto view_model = make_view_model();

        EXPECT_TRUE(view_model->isProfileLoaded());
    }

    TEST_F(SettingsViewModelTest, ProfileLoadedEmitsWhenProfileServiceNotifiesChange) {
        const auto view_model = make_view_model();
        ASSERT_TRUE(static_cast<bool>(fake_profile_service->stored_callback));

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profileLoadedChanged);
        fake_profile_service->profile_loaded = true;
        fake_profile_service->stored_callback();

        EXPECT_EQ(spy.count(), 1);
        EXPECT_TRUE(view_model->isProfileLoaded());
    }
}
