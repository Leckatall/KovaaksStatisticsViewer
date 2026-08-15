#ifndef KOVAAKSSTATISTICSVIEWER_I_SCENARIO_BROWSER_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_I_SCENARIO_BROWSER_USE_CASE_H

#include <cstddef>
#include <functional>
#include <vector>

#include <QObject>

#include "contracts/scenario_summary.h"
#include "contracts/run_performance.h"
#include "domain/scenario_perf.h"

namespace ksv::application {
    class IScenarioBrowserUseCase {
    public:
        virtual ~IScenarioBrowserUseCase() = default;

        [[nodiscard]] virtual std::vector<ScenarioSummary> getScenarioSummaries() const = 0;
        [[nodiscard]] virtual std::vector<RunPerformance> getRunsForScenario(
            const domain::ScenarioId &scenario) const = 0;
        [[nodiscard]] virtual std::vector<RunPerformance> getRecentRuns(std::size_t count) const = 0;
        [[nodiscard]] virtual domain::ScenarioPerf getCurrentPerf() const = 0;

        virtual void selectRun(const domain::ScenarioRunId &run_id) = 0;
        virtual void onChanged(QObject *context, std::function<void()> callback) = 0;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_I_SCENARIO_BROWSER_USE_CASE_H
