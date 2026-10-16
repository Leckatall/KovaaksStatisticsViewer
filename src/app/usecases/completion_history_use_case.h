#ifndef KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_USE_CASE_H

#include <QMetaObject>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "data/interfaces/i_day_source.h"
#include "data/interfaces/i_profile_service.h"
#include "contracts/i_completion_history_use_case.h"
#include "contracts/i_graph_configuration_use_case.h"
#include "contracts/playtime_view.h"
#include "contracts/range_projection.h"
#include "i_session_controller.h"

namespace ksv::application {
    class CompletionHistoryUseCase final : public ICompletionHistoryUseCase {
    public:
        CompletionHistoryUseCase(std::shared_ptr<ISessionController> session_controller,
                                 std::shared_ptr<IProfileService> profile_service,
                                 std::shared_ptr<IGraphConfigurationUseCase> configuration,
                                 std::shared_ptr<data::IDaySource> day_source)
            : m_session_controller(std::move(session_controller)),
              m_profile_service(std::move(profile_service)),
              m_configuration(std::move(configuration)),
              m_day_source(std::move(day_source)) {
            m_configuration->onChanged([this](const GraphConfigurationChange &change) {
                if (std::ranges::find(change.graphs, GraphKey::ScenarioHistory) != change.graphs.end())
                    notifyRangeChanged();
            });
            m_day_source->onDayChanged([this](std::chrono::sys_days) {
                if (followsToday(committedRange())) notifyRangeChanged();
            });
        }

        ~CompletionHistoryUseCase() override {
            QObject::disconnect(m_current_perf_connection);
            QObject::disconnect(m_profile_connection);
            QObject::disconnect(m_selected_run_connection);
        }

        CompletionHistory get_history() override {
            const auto scenario = m_session_controller->getCurrentRun().run_id.scenario_id;
            if (scenario.hash.empty()) return {};

            const auto completion_history = m_profile_service->getCompletionHistory(scenario);
            std::vector<std::chrono::sys_days> row_days;
            row_days.reserve(completion_history.size());
            for (const auto &row: completion_history) row_days.push_back(epochDayOfMillis(row.run_id.start_time));

            // The anchor comes from the scenario's full runs, before any filtering.
            const auto projection = resolveRange(committedRange(), m_day_source->today(), latestDay(row_days));
            const auto visible = selectRows(projection, row_days);

            CompletionHistory history;
            history.scenario_name = scenario.name;
            history.full_count = static_cast<int>(completion_history.size());
            history.rows.reserve(visible.size());
            for (const std::size_t i: visible) {
                const auto &row = completion_history[i];
                history.rows.push_back({
                    .run_index = static_cast<int>(i + 1),
                    .start_time_ms = row.run_id.start_time,
                    .score = row.totals.score,
                    .accuracy = row.totals.accuracy(),
                    .shots = static_cast<double>(row.totals.shots),
                    .hits = static_cast<double>(row.totals.hits),
                    .misses = static_cast<double>(row.totals.misses),
                });
            }
            return history;
        }

        void onRangeChanged(std::function<void()> callback) override {
            m_range_callbacks.push_back(std::move(callback));
        }

        void onCurrentScenarioChanged(std::function<void()> callback) override {
            QObject::disconnect(m_current_perf_connection);
            QObject::disconnect(m_profile_connection);
            m_last_scenario_hash = currentScenarioHash();

            m_current_perf_connection = QObject::connect(
                m_session_controller.get(), &ISessionController::currentRunChanged,
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

        std::optional<int> selected_run_index() override {
            const auto current = m_session_controller->getCurrentRun().run_id;
            if (current.scenario_id.hash.empty()) return std::nullopt;
            const auto completion_history = m_profile_service->getCompletionHistory(current.scenario_id);
            for (std::size_t i = 0; i < completion_history.size(); ++i)
                if (completion_history[i].run_id == current) return static_cast<int>(i + 1);
            return std::nullopt;
        }

        void onSelectedRunChanged(std::function<void()> callback) override {
            QObject::disconnect(m_selected_run_connection);
            m_selected_run_connection = QObject::connect(
                m_session_controller.get(), &ISessionController::currentRunChanged,
                m_session_controller.get(), std::move(callback));
        }

        void select_run(const int run_index) override {
            const auto scenario = m_session_controller->getCurrentRun().run_id.scenario_id;
            if (scenario.hash.empty() || run_index < 1) return;
            const auto completion_history = m_profile_service->getCompletionHistory(scenario);
            if (static_cast<std::size_t>(run_index) > completion_history.size()) return;
            m_session_controller->setCurrentRun(completion_history[static_cast<std::size_t>(run_index - 1)].run_id);
        }

    private:
        [[nodiscard]] RangePreference committedRange() const {
            return effectiveRange(m_configuration->current().configuration->preferences(GraphKey::ScenarioHistory));
        }

        void notifyRangeChanged() const {
            for (const auto &callback: m_range_callbacks) callback();
        }

        [[nodiscard]] std::string currentScenarioHash() const {
            return m_session_controller->getCurrentRun().run_id.scenario_id.hash;
        }

        std::shared_ptr<ISessionController> m_session_controller;
        std::shared_ptr<IProfileService> m_profile_service;
        std::shared_ptr<IGraphConfigurationUseCase> m_configuration;
        std::shared_ptr<data::IDaySource> m_day_source;
        std::vector<std::function<void()>> m_range_callbacks;
        std::string m_last_scenario_hash;
        QMetaObject::Connection m_current_perf_connection;
        QMetaObject::Connection m_profile_connection;
        QMetaObject::Connection m_selected_run_connection;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_COMPLETION_HISTORY_USE_CASE_H
