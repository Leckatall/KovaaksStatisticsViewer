//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SETTINGS_VM_H
#define KOVAAKSSTATSVIEWER_SETTINGS_VM_H

#include <QtCore>

#include "qt_data/interfaces/i_settings_service.h"

namespace ksv::presentation {
    class SettingsViewModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QUrl kovaaksDir READ getKovaaksDir WRITE setKovaaksDir NOTIFY kovaaksDirChanged)

    public:
        explicit SettingsViewModel(std::shared_ptr<application::ISettingsService> settings_service,
                                   QObject *parent = nullptr);

        Q_INVOKABLE [[nodiscard]] QUrl getKovaaksDir() const { return m_kovaaks_dir; }

        Q_INVOKABLE void setKovaaksDir(const QUrl &dir);

    signals:
        void kovaaksDirChanged();

    private:
        std::shared_ptr<application::ISettingsService> m_settings_service;
        QUrl m_kovaaks_dir;
    };
}

#endif //KOVAAKSSTATSVIEWER_SETTINGS_VM_H
