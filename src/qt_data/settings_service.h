//
// Created by Lecka on 02/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SETTINGS_SERVICE_H
#define KOVAAKSSTATSVIEWER_SETTINGS_SERVICE_H

#include <functional>
#include <vector>

#include <QtCore>
#include "interfaces/i_settings_service.h"

namespace ksv::qt_data {
    class SettingsService: public QObject, public application::ISettingsService {
        Q_OBJECT
        public:
        explicit SettingsService(QSettings::Format format = QSettings::NativeFormat, QObject *parent = nullptr);
        [[nodiscard]] std::string getKovaaksDir() const override;
        void setKovaaksDir(const std::string &dir) override;
        [[nodiscard]] std::string getProfilePath() const override;
        void setProfilePath(const std::string &path) override;

        void onProfilePathChanged(std::function<void()> callback) override {
            m_profile_path_callbacks.push_back(std::move(callback));
        }
        void onKovaaksDirChanged(std::function<void()> callback) override {
            m_kovaaks_dir_callbacks.push_back(std::move(callback));
        }
    private:
        [[nodiscard]] std::string readDirSetting(const QString &key, const QVariant &default_value) const;
        void writeDirSetting(const QString &key, const std::string &dir);

        QSettings m_settings;
        std::vector<std::function<void()>> m_profile_path_callbacks;
        std::vector<std::function<void()>> m_kovaaks_dir_callbacks;
    };
}

#endif //KOVAAKSSTATSVIEWER_SETTINGS_SERVICE_H
