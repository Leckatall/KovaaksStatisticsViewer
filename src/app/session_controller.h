//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H
#define KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H

#include <QObject>

#include "scenario_perf.h"
#include "data/interfaces/i_profile_service.h"
#include "interfaces/i_settings_service.h"
#include "usecases/i_session_controller.h"

namespace ksv::application {
    class SessionController : public ISessionController {
        Q_OBJECT

    public:
        explicit SessionController(std::shared_ptr<ISettingsService> settings_service,
                                   std::shared_ptr<IProfileService> profile_service, QObject *parent = nullptr);

        std::vector<domain::ScenarioId> getScenarioList() override;

        void generateProfileFromDirectory() const override;

        void setCurrentPerf(const domain::ScenarioPerf& perf) override;

        void setCurrentPerf(const std::string& filename) override;

        void setCurrentPerf(const domain::ScenarioRunId& run_id) override;

        void setCurrentPerfToLatest() override;

        [[nodiscard]] domain::ScenarioPerf getCurrentPerf() const override { return m_current_perf; }

        [[nodiscard]] std::vector<ScenarioSummary> getScenarioSummaries() const override;

        [[nodiscard]] std::vector<RunSummary> getRunsForScenario(const domain::ScenarioId& scenario) const override;

        [[nodiscard]] std::vector<RunSummary> getRecentRuns(std::size_t count) const override;

    private:
        [[nodiscard]] static RunSummary toRunSummary(const domain::ScenarioPerf& perf);

        std::shared_ptr<ISettingsService> m_settings_service;
        std::shared_ptr<IProfileService> m_profile_service;
        domain::ScenarioPerf m_current_perf;

    };
}

#endif //KOVAAKSSTATSVIEWER_SESSION_CONTROLLER_H
