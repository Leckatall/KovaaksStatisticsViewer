#ifndef KOVAAKSSTATSVIEWER_TESTS_RUN_BUILDERS_H
#define KOVAAKSSTATSVIEWER_TESTS_RUN_BUILDERS_H

#include <string>

#include "run.h"

namespace ksv::tests_support {
    // A run with only its identity and stored totals filled in — no per-sample
    // performance data. Tests that need samples build their own.
    inline domain::Run makeRun(const std::string &hash, const long long start_time, const float score = 0.0F,
                               const int shots = 0, const int hits = 0, const float duration = 0.0F) {
        domain::Run run;
        run.run_id.scenario_id = {.name = "Scenario " + hash, .hash = hash};
        run.run_id.start_time = start_time;
        run.scenario_length = duration;
        run.stored_totals = {.score = score, .shots = shots, .hits = hits, .misses = shots - hits};
        return run;
    }
}

#endif
