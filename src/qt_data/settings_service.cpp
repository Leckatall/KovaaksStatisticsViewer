//
// Created by Lecka on 02/08/2026.
//

#include "settings_service.h"

#include <QStandardPaths>

namespace ksv::qt_data {
    SettingsService::SettingsService(const QSettings::Format format, QObject *parent) : QObject(parent),
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

    std::vector<std::string> SettingsService::readDirListSetting(const QString &key) const {
        // No lock here: this is a private helper whose only caller (getKovaaksDirs) already
        // holds m_settings_mutex. m_settings_mutex is a plain QMutex (non-recursive), so
        // locking it again here would deadlock the calling thread.
        const auto stored = m_settings.value(key).toStringList();
        std::vector<std::string> result;
        result.reserve(stored.size());
        for (const auto &url: stored) result.push_back(QUrl(url).toLocalFile().toStdString());
        return result;
    }

    void SettingsService::writeDirListSetting(const QString &key, const std::vector<std::string> &dirs) {
        QStringList stored;
        stored.reserve(static_cast<qsizetype>(dirs.size()));
        for (const auto &dir: dirs) stored.push_back(QUrl::fromLocalFile(QString::fromStdString(dir)).toString());
        const QMutexLocker locker(&m_settings_mutex);
        m_settings.setValue(key, stored);
        m_settings.sync();
    }

    std::vector<std::string> SettingsService::getKovaaksDirs() const {
        static const QString kDefaultDir = "C:/Program Files (x86)/Steam/steamapps/common/FPSAimTrainer";

        const QMutexLocker locker(&m_settings_mutex);
        if (!m_settings.contains("file/kovaaksDirs")) {
            if (!m_settings.contains("file/kovaaks")) return {kDefaultDir.toStdString()};
            return {m_settings.value("file/kovaaks").toUrl().toLocalFile().toStdString()};
        }

        return readDirListSetting("file/kovaaksDirs");
    }

    bool SettingsService::isKovaaksDirSet() const {
        const QMutexLocker locker(&m_settings_mutex);
        return m_settings.contains("file/kovaaksDirs") || m_settings.contains("file/kovaaks");
    }

    void SettingsService::setKovaaksDirs(const std::vector<std::string> &dirs) {
        writeDirListSetting("file/kovaaksDirs", dirs);
        for (const auto &callback: m_kovaaks_dirs_callbacks) callback();
    }

    std::string SettingsService::getProfilePath() const {
        const auto default_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const auto default_path = QDir(default_dir).filePath("profile.pb");
        return readDirSetting("file/profilePath", QUrl::fromLocalFile(default_path));
    }

    void SettingsService::setProfilePath(const std::string &path) {
        writeDirSetting("file/profilePath", path);
        for (const auto &callback: m_profile_path_callbacks) callback();
    }
}
