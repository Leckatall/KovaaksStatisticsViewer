//
// Created by Lecka on 02/08/2026.
//

#include "settings_service.h"

namespace ksv::qt_data {
    SettingsService::SettingsService(const QSettings::Format format, QObject *parent): QObject(parent),
    m_settings(format, QSettings::UserScope, "Lecka", "KovaaksStatsViewer", this) {

    }

    std::string SettingsService::getKovaaksDir() const {
        return m_settings.value("file/kovaaks", "C:/Program Files (x86)/Steam/steamapps/common/FPSAimTrainer").toUrl().toLocalFile().toStdString();
    }
}
