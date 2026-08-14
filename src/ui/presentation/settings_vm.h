//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SETTINGS_VM_H
#define KOVAAKSSTATSVIEWER_SETTINGS_VM_H

#include <QtCore>

#include "data/interfaces/i_profile_service.h"
#include "data/interfaces/i_settings_service.h"
#include "app/usecases/i_graph_column_preferences.h"

namespace ksv::presentation {
    class SettingsViewModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QUrl kovaaksDir READ getKovaaksDir WRITE setKovaaksDir NOTIFY kovaaksDirChanged)
        Q_PROPERTY(bool kovaaksDirSet READ isKovaaksDirSet NOTIFY kovaaksDirChanged)
        Q_PROPERTY(QUrl profilePath READ getProfilePath WRITE setProfilePath NOTIFY profilePathChanged)
        Q_PROPERTY(bool profileLoaded READ isProfileLoaded NOTIFY profileLoadedChanged)

    public:
        explicit SettingsViewModel(std::shared_ptr<application::ISettingsService> settings_service,
                                   std::shared_ptr<application::IProfileService> profile_service,
                                   std::shared_ptr<application::IGraphColumnPreferences> graph_column_preferences,
                                   QObject *parent = nullptr);

        Q_INVOKABLE [[nodiscard]] QUrl getKovaaksDir() const { return m_kovaaks_dir; }
        [[nodiscard]] bool isKovaaksDirSet() const { return m_settings_service->isKovaaksDirSet(); }

        Q_INVOKABLE void setKovaaksDir(const QUrl &dir);

        Q_INVOKABLE [[nodiscard]] QUrl getProfilePath() const {
            return QUrl::fromLocalFile(m_settings_service->getProfilePath().data());
        }

        Q_INVOKABLE void setProfilePath(const QUrl &path);
        Q_INVOKABLE void setGraphColumnEnabled(int column, bool enabled);

        [[nodiscard]] bool isProfileLoaded() const { return m_profile_service->isProfileLoaded(); }

    signals:
        void kovaaksDirChanged();
        void profilePathChanged();
        void profileLoadedChanged();

    private:
        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<application::IProfileService> m_profile_service;
        std::shared_ptr<application::IGraphColumnPreferences> m_graph_column_preferences;
        QUrl m_kovaaks_dir;
    };
}

#endif //KOVAAKSSTATSVIEWER_SETTINGS_VM_H
