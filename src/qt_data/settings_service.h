//
// Created by Lecka on 02/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SETTINGS_SERVICE_H
#define KOVAAKSSTATSVIEWER_SETTINGS_SERVICE_H

#include <QtCore>
#include "interfaces/i_settings_service.h"

namespace ksv::qt_data {
    class SettingsService: public QObject, public application::ISettingsService {
        Q_OBJECT
        public:
        explicit SettingsService(QSettings::Format format = QSettings::NativeFormat, QObject *parent = nullptr);
        [[nodiscard]] std::string getKovaaksDir() const override;
        void setKovaaksDir(const std::string &dir) override;
        [[nodiscard]] std::string getProfileDir() const override;
        void setProfileDir(const std::string &dir) override;
    private:
        [[nodiscard]] std::string readDirSetting(const QString &key, const QVariant &default_value) const;
        void writeDirSetting(const QString &key, const std::string &dir);

        QSettings m_settings;
    };
}

#endif //KOVAAKSSTATSVIEWER_SETTINGS_SERVICE_H
