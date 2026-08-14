#ifndef KOVAAKSSTATSVIEWER_RUN_PERFORMANCE_H
#define KOVAAKSSTATSVIEWER_RUN_PERFORMANCE_H

#include "scenario_perf.h"

namespace ksv::domain {
    struct RunPerformance {
        ScenarioRunId run_id;
        ScenarioCompletionData completion;
    };
}

#endif //KOVAAKSSTATSVIEWER_RUN_PERFORMANCE_H
