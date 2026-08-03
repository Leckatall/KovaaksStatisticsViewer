//
// SettingsService tests. Redirects QSettings storage to a temp .ini file so
// tests never touch the real registry / user settings.
//

#include <gtest/gtest.h>

#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>

#include "settings_service.h"

using namespace ksv::qt_data;

namespace {
    // Uses SettingsService(QSettings::IniFormat) + QSettings::setPath so these
    // tests read/write a temp .ini file instead of the real Windows registry
    // (SettingsService's default constructor argument, QSettings::NativeFormat,
    // is what production code uses and must never be touched by tests).
    class SettingsServiceTest : public testing::Test {
    protected:
        QTemporaryDir temp_dir;

        void SetUp() override {
            ASSERT_TRUE(temp_dir.isValid());
            QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir.path());
        }
    };

    TEST_F(SettingsServiceTest, ReturnsDefaultWhenUnset) {
        const QString default_dir = "C:/Program Files(x86)/Steam/steamapps/common/FPSAimTrainer";
        const std::string expected = QUrl(default_dir).toLocalFile().toStdString();

        const SettingsService settings(QSettings::IniFormat);

        EXPECT_EQ(settings.getKovaaksDir(), expected);
    }

    TEST_F(SettingsServiceTest, ReturnsPreviouslyStoredValue) {
        {
            QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
            raw.setValue("file/kovaaks", QUrl::fromLocalFile("D:/Games/FPSAimTrainer"));
        }

        const SettingsService settings(QSettings::IniFormat);

        EXPECT_EQ(settings.getKovaaksDir(), QUrl::fromLocalFile("D:/Games/FPSAimTrainer").toLocalFile().toStdString());
    }

    TEST_F(SettingsServiceTest, SetKovaaksDirPersistsValue) {
        SettingsService settings(QSettings::IniFormat);

        settings.setKovaaksDir("D:/Games/FPSAimTrainer");

        const SettingsService reloaded(QSettings::IniFormat);
        EXPECT_EQ(reloaded.getKovaaksDir(), QUrl::fromLocalFile("D:/Games/FPSAimTrainer").toLocalFile().toStdString());
    }
}
