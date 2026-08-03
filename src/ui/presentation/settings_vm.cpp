//
// Created by Lecka on 03/08/2026.
//

#include "settings_vm.h"

namespace ksv::presentation {
    SettingsViewModel::SettingsViewModel(
        std::shared_ptr<application::ISettingsService> settings_service,
        QObject *parent) : QObject(parent),
                           m_settings_service(std::move(settings_service)),
                           m_kovaaks_dir(QUrl::fromLocalFile(m_settings_service->getKovaaksDir().data())) {
    }

    void SettingsViewModel::setKovaaksDir(const QUrl &dir) {
        if (dir == m_kovaaks_dir) return;
        m_settings_service->setKovaaksDir(dir.toLocalFile().toStdString());
        m_kovaaks_dir = dir;
        emit kovaaksDirChanged();
    }
}
