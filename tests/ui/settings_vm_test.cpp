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
        std::string profile_dir = "C:/Profile";

        [[nodiscard]] std::string getKovaaksDir() const override { return dir; }
        void setKovaaksDir(const std::string &new_dir) override { dir = new_dir; }
        [[nodiscard]] std::string getProfileDir() const override { return profile_dir; }
        void setProfileDir(const std::string &new_dir) override { profile_dir = new_dir; }
    };

    class FakeProfileService : public IProfileService {
    public:
        bool profile_loaded = false;
        std::string profile_directory;
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

        [[nodiscard]] std::optional<float> getAverageScore(
            const ScenarioId &, std::size_t) const override { return std::nullopt; }

        [[nodiscard]] bool isProfileLoaded() const override { return profile_loaded; }

        void setProfileDirectory(const std::string &dir) override { profile_directory = dir; }

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

    TEST_F(SettingsViewModelTest, ProfileDirReflectsServiceValueAtConstruction) {
        fake_service->profile_dir = "D:/CustomProfile";
        const auto view_model = make_view_model();

        EXPECT_EQ(view_model->getProfileDir(), QUrl::fromLocalFile("D:/CustomProfile"));
    }

    TEST_F(SettingsViewModelTest, SetProfileDirUpdatesSettingsServiceAndEmitsOnChange) {
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profileDirChanged);
        view_model->setProfileDir(QUrl::fromLocalFile("D:/NewProfile"));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model->getProfileDir(), QUrl::fromLocalFile("D:/NewProfile"));
        EXPECT_EQ(fake_service->profile_dir, "D:/NewProfile");
    }

    TEST_F(SettingsViewModelTest, SetProfileDirDoesNotEmitWhenUnchanged) {
        fake_service->profile_dir = "C:/Profile";
        const auto view_model = make_view_model();

        const QSignalSpy spy(view_model.get(), &SettingsViewModel::profileDirChanged);
        view_model->setProfileDir(QUrl::fromLocalFile("C:/Profile"));

        EXPECT_EQ(spy.count(), 0);
    }

    TEST_F(SettingsViewModelTest, SetProfileDirRepointsProfileService) {
        const auto view_model = make_view_model();

        view_model->setProfileDir(QUrl::fromLocalFile("D:/NewProfile"));

        EXPECT_EQ(fake_profile_service->profile_directory, "D:/NewProfile");
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
