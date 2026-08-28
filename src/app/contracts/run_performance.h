#ifndef KOVAAKSSTATISTICSVIEWER_APP_RUN_PERFORMANCE_H
#define KOVAAKSSTATISTICSVIEWER_APP_RUN_PERFORMANCE_H

#include "domain/run.h"

namespace ksv::application {
    struct RunPerformance {
        domain::RunSummary data;
        bool personal_best = false;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_APP_RUN_PERFORMANCE_H
