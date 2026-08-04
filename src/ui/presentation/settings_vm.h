//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SETTINGS_VM_H
#define KOVAAKSSTATSVIEWER_SETTINGS_VM_H

#include <QtCore>

#include "data/interfaces/i_profile_service.h"
#include "qt_data/interfaces/i_settings_service.h"

namespace ksv::presentation {
    class SettingsViewModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QUrl kovaaksDir READ getKovaaksDir WRITE setKovaaksDir NOTIFY kovaaksDirChanged)
        Q_PROPERTY(QUrl profileDir READ getProfileDir WRITE setProfileDir NOTIFY profileDirChanged)
        Q_PROPERTY(bool profileLoaded READ isProfileLoaded NOTIFY profileLoadedChanged)

    public:
        explicit SettingsViewModel(std::shared_ptr<application::ISettingsService> settings_service,
                                   std::shared_ptr<application::IProfileService> profile_service,
                                   QObject *parent = nullptr);

        Q_INVOKABLE [[nodiscard]] QUrl getKovaaksDir() const { return m_kovaaks_dir; }

        Q_INVOKABLE void setKovaaksDir(const QUrl &dir);

        Q_INVOKABLE [[nodiscard]] QUrl getProfileDir() const {
            return QUrl::fromLocalFile(m_settings_service->getProfileDir().data());
        }

        Q_INVOKABLE void setProfileDir(const QUrl &dir);

        [[nodiscard]] bool isProfileLoaded() const { return m_profile_service->isProfileLoaded(); }

    signals:
        void kovaaksDirChanged();
        void profileDirChanged();
        void profileLoadedChanged();

    private:
        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<application::IProfileService> m_profile_service;
        QUrl m_kovaaks_dir;
    };
}

#endif //KOVAAKSSTATSVIEWER_SETTINGS_VM_H
