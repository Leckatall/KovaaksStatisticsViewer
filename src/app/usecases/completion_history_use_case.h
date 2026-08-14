#ifndef KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_USE_CASE_H

#include <QMetaObject>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "data/interfaces/i_profile_service.h"
#include "contracts/i_completion_history_use_case.h"
#include "i_session_controller.h"

namespace ksv::application {
    class CompletionHistoryUseCase final : public ICompletionHistoryUseCase {
    public:
        CompletionHistoryUseCase(std::shared_ptr<ISessionController> session_controller,
                                 std::shared_ptr<IProfileService> profile_service)
            : m_session_controller(std::move(session_controller)),
              m_profile_service(std::move(profile_service)) {
        }

        ~CompletionHistoryUseCase() override {
            QObject::disconnect(m_current_perf_connection);
            QObject::disconnect(m_profile_connection);
        }

        CompletionHistory get_history() override {
            const auto scenario = m_session_controller->getCurrentPerf().run_id.scenario_id;
            if (scenario.hash.empty()) return {};

            CompletionHistory history;
            history.scenario_name = scenario.name;
            const auto completion_history = m_profile_service->getCompletionHistory(scenario);
            history.rows.reserve(completion_history.size());
            for (std::size_t i = 0; i < completion_history.size(); ++i) {
                const auto &[run_id, completion] = completion_history[i];
                history.rows.push_back({
                    .run_index = static_cast<int>(i + 1),
                    .start_time_ms = run_id.start_time,
                    .score = completion.score,
                    .accuracy = completion.accuracy(),
                    .shots = static_cast<double>(completion.shots),
                    .hits = static_cast<double>(completion.hits),
                    .misses = static_cast<double>(completion.misses),
                });
            }
            return history;
        }

        void onCurrentScenarioChanged(std::function<void()> callback) override {
            QObject::disconnect(m_current_perf_connection);
            QObject::disconnect(m_profile_connection);
            m_last_scenario_hash = currentScenarioHash();

            m_current_perf_connection = QObject::connect(
                m_session_controller.get(), &ISessionController::currentPerfChanged,
                m_session_controller.get(), [this, callback] {
                    const std::string scenario_hash = currentScenarioHash();
                    if (scenario_hash == m_last_scenario_hash) return;
                    m_last_scenario_hash = scenario_hash;
                    callback();
                });
            m_profile_connection = QObject::connect(
                m_session_controller.get(), &ISessionController::profileChanged,
                m_session_controller.get(), [this, callback = std::move(callback)] {
                    m_last_scenario_hash = currentScenarioHash();
                    callback();
                });
        }

    private:
        [[nodiscard]] std::string currentScenarioHash() const {
            return m_session_controller->getCurrentPerf().run_id.scenario_id.hash;
        }

        std::shared_ptr<ISessionController> m_session_controller;
        std::shared_ptr<IProfileService> m_profile_service;
        std::string m_last_scenario_hash;
        QMetaObject::Connection m_current_perf_connection;
        QMetaObject::Connection m_profile_connection;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_USE_CASE_H
