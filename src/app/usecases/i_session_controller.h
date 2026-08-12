//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
#define KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H

#include <string>
#include <vector>

#include "domain/scenario_perf.h"
#include "run_summary.h"
#include <QObject>

namespace ksv::application {
    class ISessionController : public QObject {
        Q_OBJECT

    public:
        explicit ISessionController(QObject *parent = nullptr) : QObject(parent) {
        }

        virtual std::vector<domain::ScenarioId> getScenarioList() = 0;

        virtual void generateProfileFromDirectory() = 0;

        virtual void setCurrentPerfToLatest() = 0;

        virtual void setCurrentPerf(const domain::ScenarioPerf &perf) = 0;

        virtual void setCurrentPerf(const std::string &filename) = 0;

        virtual void setCurrentPerf(const domain::ScenarioRunId &run_id) = 0;

        [[nodiscard]] virtual domain::ScenarioPerf getCurrentPerf() const = 0;

        // A build started before a view model existed still has to show up in it.
        [[nodiscard]] virtual bool isBuildInProgress() const = 0;

        [[nodiscard]] virtual std::vector<ScenarioSummary> getScenarioSummaries() const = 0;

        // Newest-first.
        [[nodiscard]] virtual std::vector<RunSummary> getRunsForScenario(
            const domain::ScenarioId &scenario) const = 0;

        // Newest-first, capped at count, across all scenarios.
        [[nodiscard]] virtual std::vector<RunSummary> getRecentRuns(std::size_t count) const = 0;

    signals:
        void currentPerfChanged();
        void profileChanged();

        void buildStarted();
        void buildProgress(int done, int total);
        void buildFinished();
    };
}


#endif //KOVAAKSSTATSVIEWER_I_SESSION_CONTROLLER_H
