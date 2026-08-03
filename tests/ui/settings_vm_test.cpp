//
// SettingsViewModel tests using a hand-written fake ISettingsService.
//

#include <gtest/gtest.h>

#include <QSignalSpy>

#include "settings_vm.h"

using namespace ksv::presentation;
using namespace ksv::application;

namespace {
    class FakeSettingsService : public ISettingsService {
    public:
        std::string dir = "C:/Kovaaks";

        [[nodiscard]] std::string getKovaaksDir() const override { return dir; }
        void setKovaaksDir(const std::string &new_dir) override { dir = new_dir; }
    };

    class SettingsViewModelTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSettingsService> fake_service = std::make_shared<FakeSettingsService>();
    };

    TEST_F(SettingsViewModelTest, KovaaksDirReflectsServiceValueAtConstruction) {
        fake_service->dir = "D:/CustomDir";
        const SettingsViewModel view_model(fake_service);

        EXPECT_EQ(view_model.getKovaaksDir(), QUrl::fromLocalFile("D:/CustomDir"));
    }

    TEST_F(SettingsViewModelTest, SetKovaaksDirUpdatesServiceAndEmitsOnChange) {
        SettingsViewModel view_model(fake_service);

        const QSignalSpy spy(&view_model, &SettingsViewModel::kovaaksDirChanged);
        view_model.setKovaaksDir(QUrl::fromLocalFile("D:/NewDir"));

        EXPECT_EQ(spy.count(), 1);
        EXPECT_EQ(view_model.getKovaaksDir(), QUrl::fromLocalFile("D:/NewDir"));
        EXPECT_EQ(fake_service->dir, "D:/NewDir");
    }

    TEST_F(SettingsViewModelTest, SetKovaaksDirDoesNotEmitWhenUnchanged) {
        fake_service->dir = "C:/Kovaaks";
        SettingsViewModel view_model(fake_service);

        const QSignalSpy spy(&view_model, &SettingsViewModel::kovaaksDirChanged);
        view_model.setKovaaksDir(QUrl::fromLocalFile("C:/Kovaaks"));

        EXPECT_EQ(spy.count(), 0);
    }
}
