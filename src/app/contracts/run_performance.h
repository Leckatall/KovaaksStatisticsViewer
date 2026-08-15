#ifndef KOVAAKSSTATISTICSVIEWER_APP_RUN_PERFORMANCE_H
#define KOVAAKSSTATISTICSVIEWER_APP_RUN_PERFORMANCE_H

#include "domain/scenario_perf.h"

namespace ksv::application {
    struct RunPerformance {
        domain::RunData data;
        bool personal_best = false;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_APP_RUN_PERFORMANCE_H
