//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SESSION_VM_H
#define KOVAAKSSTATSVIEWER_SESSION_VM_H

#include <QtCore>

#include "app/usecases/i_session_controller.h"

namespace ksv::presentation {
    class SessionViewModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QStringList scenario_list READ getScenarioList NOTIFY scenario_list_changed)
        Q_PROPERTY(QUrl kovaaks_dir READ getKovaaksDir NOTIFY kovaaksDirChanged)

    public:
        explicit SessionViewModel(std::shared_ptr<application::ISessionController> session_controller,
                                  QObject *parent = nullptr);

        void updateScenarioHashMap();

        Q_INVOKABLE [[nodiscard]] QStringList getScenarioList();

        void updateKovaaksDir() {
            if (const auto new_dir = QUrl::fromLocalFile(m_session_controller->getKovaaksDir().data());
                new_dir != m_kovaaks_dir) {
                emit kovaaksDirChanged();
                m_kovaaks_dir = new_dir;
            }
        }

        Q_INVOKABLE [[nodiscard]] QUrl getKovaaksDir() const { return m_kovaaks_dir; }

        Q_INVOKABLE void generateProfile() { m_session_controller->generateProfileFromDirectory(); updateScenarioHashMap();}

    signals:
        void scenario_list_changed();

        void kovaaksDirChanged();

    private:
        std::shared_ptr<application::ISessionController> m_session_controller;
        QUrl m_kovaaks_dir;
        QMap<QString, QString> m_scenario_hash_to_name;
    };
}

#endif //KOVAAKSSTATSVIEWER_SESSION_VM_H
