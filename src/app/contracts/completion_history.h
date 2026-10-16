#ifndef KOVAAKSSTATSVIEWER_COMPLETION_HISTORY_H
#define KOVAAKSSTATSVIEWER_COMPLETION_HISTORY_H

#include <string>
#include <vector>

namespace ksv::application {
    struct CompletionRow {
        int run_index = 0;
        long long start_time_ms = 0;
        double score = 0.0;
        double accuracy = 0.0;
        double shots = 0.0;
        double hits = 0.0;
        double misses = 0.0;
    };

    struct CompletionHistory {
        std::string scenario_name;
        // Every run of the scenario, before the range filter; rows may be a subset of them.
        int full_count = 0;
        // The visible rows, oldest first, each keeping its run_index in the full history.
        std::vector<CompletionRow> rows;
    };
}

#endif //KOVAAKSSTATSVIEWER_COMPLETION_HISTORY_H
