//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
#define KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H

#include <string>
#include <vector>

#include "domain/run.h"
#include <QObject>

namespace ksv::application {
    class ISessionController : public QObject {
        Q_OBJECT

    public:
        explicit ISessionController(QObject *parent = nullptr) : QObject(parent) {
        }

        virtual std::vector<domain::ScenarioId> getScenarioList() = 0;

        virtual void generateProfileFromDirectory() = 0;

        virtual void setCurrentRunToLatest() = 0;

        virtual void setCurrentRun(const domain::Run &run) = 0;

        virtual void setCurrentPerf(const std::string &filename) = 0;

        virtual void setCurrentRun(const domain::ScenarioRunId &run_id) = 0;

        [[nodiscard]] virtual domain::Run getCurrentRun() const = 0;

        // A build started before a view model existed still has to show up in it.
        [[nodiscard]] virtual bool isBuildInProgress() const = 0;

    signals:
        void currentRunChanged();
        void profileChanged();

        void buildStarted();
        void buildProgress(int done, int total);
        void buildFinished();
    };
}


#endif //KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
