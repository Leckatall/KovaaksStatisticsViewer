//
// SettingsService tests. Redirects QSettings storage to a temp .ini file so
// tests never touch the real registry / user settings.
//

#include <gtest/gtest.h>

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
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


    TEST_F(SettingsServiceTest, ReturnsPreviouslyStoredValue) {
        {
            QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
            raw.setValue("file/kovaaks", QUrl::fromLocalFile("D:/Games/FPSAimTrainer"));
        }

        const SettingsService settings(QSettings::IniFormat);

        EXPECT_EQ(settings.getKovaaksDirs(),
                  (std::vector<std::string>{QUrl::fromLocalFile("D:/Games/FPSAimTrainer").toLocalFile().toStdString()}));
    }

    TEST_F(SettingsServiceTest, SetKovaaksDirsPersistsEveryValue) {
        SettingsService settings(QSettings::IniFormat);

        settings.setKovaaksDirs({"D:/Games/First", "E:/Games/Second"});

        const SettingsService reloaded(QSettings::IniFormat);
        EXPECT_EQ(reloaded.getKovaaksDirs(), (std::vector<std::string>{"D:/Games/First", "E:/Games/Second"}));
    }

    TEST_F(SettingsServiceTest, NewListSettingTakesPrecedenceOverLegacyValue) {
        QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        raw.setValue("file/kovaaks", QUrl::fromLocalFile("C:/Legacy"));
        raw.setValue("file/kovaaksDirs", QStringList{QUrl::fromLocalFile("D:/Current").toString()});

        const SettingsService settings(QSettings::IniFormat);

        EXPECT_EQ(settings.getKovaaksDirs(), (std::vector<std::string>{"D:/Current"}));
    }

    TEST_F(SettingsServiceTest, IsKovaaksDirSetFalseWhenNeverConfigured) {
        const SettingsService settings(QSettings::IniFormat);

        EXPECT_FALSE(settings.isKovaaksDirSet());
    }

    TEST_F(SettingsServiceTest, IsKovaaksDirSetTrueAfterSetKovaaksDir) {
        SettingsService settings(QSettings::IniFormat);

        settings.setKovaaksDirs({"D:/Games/FPSAimTrainer"});

        EXPECT_TRUE(settings.isKovaaksDirSet());
    }

    TEST_F(SettingsServiceTest, IsKovaaksDirSetTrueForLegacyKey) {
        QSettings raw(QSettings::IniFormat, QSettings::UserScope, "Lecka", "KovaaksStatsViewer");
        raw.setValue("file/kovaaks", QUrl::fromLocalFile("C:/Legacy"));

        const SettingsService settings(QSettings::IniFormat);

        EXPECT_TRUE(settings.isKovaaksDirSet());
    }

    TEST_F(SettingsServiceTest, ReturnsAppDataProfileFileForProfilePathWhenUnset) {
        const SettingsService settings(QSettings::IniFormat);

        const auto app_data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const auto expected = QDir(app_data).filePath("profile.pb").toStdString();
        EXPECT_EQ(settings.getProfilePath(), expected);
    }

    TEST_F(SettingsServiceTest, SetProfilePathPersistsValue) {
        SettingsService settings(QSettings::IniFormat);

        settings.setProfilePath("D:/Games/Profile/profile.pb");

        const SettingsService reloaded(QSettings::IniFormat);
        EXPECT_EQ(reloaded.getProfilePath(),
                  QUrl::fromLocalFile("D:/Games/Profile/profile.pb").toLocalFile().toStdString());
    }

    TEST_F(SettingsServiceTest, SetProfilePathNotifiesRegisteredObservers) {
        SettingsService settings(QSettings::IniFormat);

        int notify_count = 0;
        settings.onProfilePathChanged([&notify_count] { ++notify_count; });

        settings.setProfilePath("D:/Games/Profile/profile.pb");

        EXPECT_EQ(notify_count, 1);
        // The observer sees the freshly stored value when it runs.
        EXPECT_EQ(settings.getProfilePath(),
                  QUrl::fromLocalFile("D:/Games/Profile/profile.pb").toLocalFile().toStdString());
    }
}
