//
// Created by Lecka on 02/08/2026.
//

#include "settings_service.h"

#include <QStandardPaths>

namespace ksv::qt_data {
    SettingsService::SettingsService(const QSettings::Format format, QObject *parent): QObject(parent),
    m_settings(format, QSettings::UserScope, "Lecka", "KovaaksStatsViewer", this) {

    }

    std::string SettingsService::readDirSetting(const QString &key, const QVariant &default_value) const {
        return m_settings.value(key, default_value).toUrl().toLocalFile().toStdString();
    }

    void SettingsService::writeDirSetting(const QString &key, const std::string &dir) {
        m_settings.setValue(key, QUrl::fromLocalFile(QString::fromStdString(dir)));
        m_settings.sync();
    }

    std::string SettingsService::getKovaaksDir() const {
        return readDirSetting("file/kovaaks", "C:/Program Files (x86)/Steam/steamapps/common/FPSAimTrainer");
    }

    void SettingsService::setKovaaksDir(const std::string &dir) {
        writeDirSetting("file/kovaaks", dir);
    }

    std::string SettingsService::getProfileDir() const {
        const auto default_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return readDirSetting("file/profileDir", QUrl::fromLocalFile(default_dir));
    }

    void SettingsService::setProfileDir(const std::string &dir) {
        writeDirSetting("file/profileDir", dir);
    }
}
