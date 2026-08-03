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

    public:
        explicit SessionViewModel(std::shared_ptr<application::ISessionController> session_controller,
                                  QObject *parent = nullptr);

        void updateScenarioHashMap();

        Q_INVOKABLE [[nodiscard]] QStringList getScenarioList();

        Q_INVOKABLE void generateProfile() { m_session_controller->generateProfileFromDirectory(); updateScenarioHashMap();}

        Q_INVOKABLE [[nodiscard]] domain::ScenarioPerf getCurrentPerf() const { return m_session_controller->getCurrentPerf(); }
        Q_INVOKABLE [[nodiscard]] QString getCurrentPerfScenario() const { return getCurrentPerf().run_id.scenario_id.name.data(); }

    signals:
        void scenario_list_changed();

    private:
        std::shared_ptr<application::ISessionController> m_session_controller;
        QMap<QString, QString> m_scenario_hash_to_name;
    };
}

#endif //KOVAAKSSTATSVIEWER_SESSION_VM_H
