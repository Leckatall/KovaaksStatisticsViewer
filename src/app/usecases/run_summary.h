//
// Created by Lecka on 09/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_RUN_SUMMARY_H
#define KOVAAKSSTATSVIEWER_RUN_SUMMARY_H

#include <QString>

#include "domain/scenario_perf.h"

namespace ksv::application {
    struct RunSummary {
        domain::ScenarioRunId run_id;
        QString scenario_name;
        qint64 start_time_ms = 0;
        float score = 0.0F;
        float accuracy = 0.0F;
        float duration_seconds = 0.0F;
        int shots = 0;
        int hits = 0;
    };

    struct ScenarioSummary {
        domain::ScenarioId scenario_id;
        int run_count = 0;
        double total_time_seconds = 0.0;
        std::chrono::sys_seconds last_played;
    };
}

#endif //KOVAAKSSTATSVIEWER_RUN_SUMMARY_H
