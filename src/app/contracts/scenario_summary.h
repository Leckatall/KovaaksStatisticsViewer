//
// Created by Lecka on 09/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SCENARIO_SUMMARY_H
#define KOVAAKSSTATSVIEWER_SCENARIO_SUMMARY_H

#include <chrono>

#include "domain/run.h"

namespace ksv::application {
    struct ScenarioSummary {
        domain::ScenarioId scenario_id;
        int run_count = 0;
        double total_time_seconds = 0.0;
        std::chrono::sys_seconds last_played;
    };
}

#endif //KOVAAKSSTATSVIEWER_SCENARIO_SUMMARY_H
