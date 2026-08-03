//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
#define KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H

#include <string>
#include <vector>

#include "domain/scenario_perf.h"
#include <QObject>

namespace ksv::application {
    class ISessionController : public QObject {
        Q_OBJECT

    public:
        explicit ISessionController(QObject *parent = nullptr) : QObject(parent) {
        }

        virtual std::vector<domain::ScenarioId> getScenarioList() = 0;

        virtual void generateProfileFromDirectory() const = 0;

        virtual void setCurrentPerf(const domain::ScenarioPerf &perf) = 0;

        virtual void setCurrentPerf(const std::string &filename) = 0;

        [[nodiscard]] virtual domain::ScenarioPerf getCurrentPerf() const = 0;

    signals:
        void currentPerfChanged();
    };
}


#endif //KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
