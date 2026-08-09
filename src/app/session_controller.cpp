//
// Created by Lecka on 01/08/2026.
//

#include "session_controller.h"

namespace ksv::application {
    SessionController::SessionController(std::shared_ptr<ISettingsService> settings_service,
                                         std::shared_ptr<IProfileService> profile_service,
                                         QObject *parent) : ISessionController(parent),
                                                            m_settings_service(std::move(settings_service)),
                                                            m_profile_service(std::move(profile_service)) {
        setCurrentPerfToLatest();
        m_profile_service->onProfileChanged([this] { setCurrentPerfToLatest(); });
    }

    std::vector<domain::ScenarioId> SessionController::getScenarioList() {
        return m_profile_service->getScenarioList();
    }

    void SessionController::generateProfileFromDirectory() const {
        m_profile_service->generateProfileFromDirectory();
    }

    void SessionController::setCurrentPerf(const domain::ScenarioPerf &perf) {
        if (m_current_perf.run_id == perf.run_id) return;
        m_current_perf = perf;
        emit currentPerfChanged();
    }

    void SessionController::setCurrentPerf(const std::string &filename) {
        setCurrentPerf(m_profile_service->getPerf(filename));
    }

    void SessionController::setCurrentPerf(const domain::ScenarioRunId &run_id) {
        if (const auto perf = m_profile_service->getRun(run_id)) {
            setCurrentPerf(*perf);
        }
    }

    void SessionController::setCurrentPerfToLatest() {
        setCurrentPerf(m_profile_service->getLatestPerf());
    }

    RunSummary SessionController::toRunSummary(const domain::ScenarioPerf &perf) {
        const auto completion = perf.getCompletionData();
        RunSummary summary;
        summary.run_id = perf.run_id;
        summary.scenario_name = QString::fromStdString(perf.run_id.scenario_id.name);
        summary.start_time_ms = perf.run_id.start_time;
        summary.score = completion.score;
        summary.accuracy = completion.shots == 0 ? 0.0F : static_cast<float>(completion.hits) / static_cast<float>(completion.shots);
        summary.duration_seconds = perf.scenario_length;
        summary.shots = completion.shots;
        summary.hits = completion.hits;
        return summary;
    }

    std::vector<ScenarioSummary> SessionController::getScenarioSummaries() const {
        std::vector<ScenarioSummary> summaries;
        const auto scenarios = m_profile_service->getScenarioList();
        summaries.reserve(scenarios.size());
        for (const auto &scenario: scenarios) {
            ScenarioSummary summary;
            summary.scenario_id = scenario;
            summary.run_count = static_cast<int>(m_profile_service->getRunCount(scenario).value_or(0));
            summary.total_time_seconds = m_profile_service->getTotalTime(scenario).value_or(0.0);
            summaries.push_back(summary);
        }
        return summaries;
    }

    std::vector<RunSummary> SessionController::getRunsForScenario(const domain::ScenarioId &scenario) const {
        const auto count = m_profile_service->getRunCount(scenario).value_or(0);
        const auto perfs = m_profile_service->getMostRecentPerfs(scenario, count);

        std::vector<RunSummary> summaries;
        summaries.reserve(perfs.size());
        for (auto it = perfs.rbegin(); it != perfs.rend(); ++it) {
            summaries.push_back(toRunSummary(*it));
        }
        return summaries;
    }

    std::vector<RunSummary> SessionController::getRecentRuns(const std::size_t count) const {
        const auto perfs = m_profile_service->getRecentRuns(count);

        std::vector<RunSummary> summaries;
        summaries.reserve(perfs.size());
        for (const auto &perf: perfs) {
            summaries.push_back(toRunSummary(perf));
        }
        return summaries;
    }
}
