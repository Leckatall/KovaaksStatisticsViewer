#ifndef KOVAAKSSTATISTICSVIEWER_SCENARIO_BROWSER_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_SCENARIO_BROWSER_USE_CASE_H

#include <QMetaObject>

#include <iostream>
#include <memory>
#include <utility>

#include "contracts/i_scenario_browser_use_case.h"
#include "data/interfaces/i_profile_service.h"
#include "i_session_controller.h"

namespace ksv::application {
    class ScenarioBrowserUseCase final : public IScenarioBrowserUseCase {
    public:
        ScenarioBrowserUseCase(std::shared_ptr<ISessionController> session_controller,
                              std::shared_ptr<IProfileService> profile_service)
            : m_session_controller(std::move(session_controller)),
              m_profile_service(std::move(profile_service)) {
        }

        ~ScenarioBrowserUseCase() override {
            QObject::disconnect(m_current_perf_connection);
            QObject::disconnect(m_profile_connection);
        }

        [[nodiscard]] std::vector<ScenarioSummary> getScenarioSummaries() const override {
            std::vector<ScenarioSummary> summaries;
            const auto scenarios = m_profile_service->getScenarioList();
            summaries.reserve(scenarios.size());
            for (const auto &scenario: scenarios) {
                ScenarioSummary summary;
                summary.scenario_id = scenario;
                summary.run_count = static_cast<int>(m_profile_service->getRunCount(scenario).value_or(0));
                summary.total_time_seconds = m_profile_service->getTotalTime(scenario).value_or(0.0);
                if (m_profile_service->getLastRunTime(scenario)) {
                    summary.last_played = m_profile_service->getLastRunTime(scenario).value();
                } else {
                    std::cerr << "No last run time for scenario '" << scenario.name << "'" << std::endl;
                }
                summaries.push_back(summary);
            }
            return summaries;
        }

        [[nodiscard]] std::vector<domain::RunPerformance> getRunsForScenario(
            const domain::ScenarioId &scenario) const override {
            const auto count = m_profile_service->getRunCount(scenario).value_or(0);
            const auto perfs = m_profile_service->getMostRecentPerfs(scenario, count);

            std::vector<domain::RunPerformance> summaries;
            summaries.reserve(perfs.size());
            for (auto it = perfs.rbegin(); it != perfs.rend(); ++it) {
                summaries.push_back({it->run_id, it->getCompletionData()});
            }
            return summaries;
        }

        [[nodiscard]] std::vector<domain::RunPerformance> getRecentRuns(const std::size_t count) const override {
            const auto perfs = m_profile_service->getRecentRuns(count);

            std::vector<domain::RunPerformance> summaries;
            summaries.reserve(perfs.size());
            for (const auto &perf: perfs) {
                summaries.push_back({perf.run_id, perf.getCompletionData()});
            }
            return summaries;
        }

        [[nodiscard]] domain::ScenarioPerf getCurrentPerf() const override {
            return m_session_controller->getCurrentPerf();
        }

        void selectRun(const domain::ScenarioRunId &run_id) override {
            m_session_controller->setCurrentPerf(run_id);
        }

        void onChanged(QObject *context, std::function<void()> callback) override {
            QObject::disconnect(m_current_perf_connection);
            QObject::disconnect(m_profile_connection);
            m_current_perf_connection = QObject::connect(
                m_session_controller.get(), &ISessionController::currentPerfChanged,
                context, callback);
            m_profile_connection = QObject::connect(
                m_session_controller.get(), &ISessionController::profileChanged,
                context, std::move(callback));
        }

    private:
        std::shared_ptr<ISessionController> m_session_controller;
        std::shared_ptr<IProfileService> m_profile_service;
        QMetaObject::Connection m_current_perf_connection;
        QMetaObject::Connection m_profile_connection;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_SCENARIO_BROWSER_USE_CASE_H
