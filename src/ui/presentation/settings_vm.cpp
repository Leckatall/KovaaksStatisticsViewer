//
// Created by Lecka on 03/08/2026.
//

#include "settings_vm.h"

namespace ksv::presentation {
    SettingsViewModel::SettingsViewModel(
        std::shared_ptr<application::ISettingsService> settings_service,
        std::shared_ptr<application::IProfileService> profile_service,
        QObject *parent) : QObject(parent),
                           m_settings_service(std::move(settings_service)),
                           m_profile_service(std::move(profile_service)),
                           m_kovaaks_dir(QUrl::fromLocalFile(m_settings_service->getKovaaksDir().data())) {
        const QPointer<SettingsViewModel> self(this);
        m_profile_service->onProfileChanged([self] { if (self) emit self->profileLoadedChanged(); });
    }

    void SettingsViewModel::setKovaaksDir(const QUrl &dir) {
        if (dir == m_kovaaks_dir) return;
        m_settings_service->setKovaaksDir(dir.toLocalFile().toStdString());
        m_kovaaks_dir = dir;
        emit kovaaksDirChanged();
    }

    void SettingsViewModel::setProfilePath(const QUrl &path) {
        if (path == getProfilePath()) return;
        // Setting path triggers ProfileService to repoint the store and reload.
        m_settings_service->setProfilePath(path.toLocalFile().toStdString());
        emit profilePathChanged();
    }
}
