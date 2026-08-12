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
        const QMutexLocker locker(&m_settings_mutex);
        return m_settings.value(key, default_value).toUrl().toLocalFile().toStdString();
    }

    void SettingsService::writeDirSetting(const QString &key, const std::string &dir) {
        const QMutexLocker locker(&m_settings_mutex);
        m_settings.setValue(key, QUrl::fromLocalFile(QString::fromStdString(dir)));
        m_settings.sync();
    }

    std::string SettingsService::getKovaaksDir() const {
        return readDirSetting("file/kovaaks", "C:/Program Files (x86)/Steam/steamapps/common/FPSAimTrainer");
    }

    bool SettingsService::isKovaaksDirSet() const {
        const QMutexLocker locker(&m_settings_mutex);
        return m_settings.contains("file/kovaaks");
    }

    void SettingsService::setKovaaksDir(const std::string &dir) {
        writeDirSetting("file/kovaaks", dir);
        for (const auto &callback: m_kovaaks_dir_callbacks) callback();
    }

    std::string SettingsService::getProfilePath() const {
        const auto default_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const auto default_path = QDir(default_dir).filePath("profile_cache.pb");
        return readDirSetting("file/profilePath", QUrl::fromLocalFile(default_path));
    }

    void SettingsService::setProfilePath(const std::string &path) {
        writeDirSetting("file/profilePath", path);
        for (const auto &callback: m_profile_path_callbacks) callback();
    }
}
