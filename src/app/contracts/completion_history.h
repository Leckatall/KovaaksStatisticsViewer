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
        std::vector<CompletionRow> rows;
    };
}

#endif //KOVAAKSSTATSVIEWER_COMPLETION_HISTORY_H
