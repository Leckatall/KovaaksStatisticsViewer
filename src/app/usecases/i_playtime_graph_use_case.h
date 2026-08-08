//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
#include <utility>
#include <vector>

namespace ksv::application {
    class IPlaytimeGraphUseCase {
    public:
        virtual ~IPlaytimeGraphUseCase() = default;

        // Rolling average of daily playtime over a trailing `window_days`
        // window, across all scenarios. Each pair is
        // (days-since-Unix-epoch, average-seconds-per-day). Ordered by day,
        // one entry per calendar day from the first to the last day with runs.
        // Empty if no profile is loaded or it has no runs.
        virtual std::vector<std::pair<long long, double>> get_rolling_playtime(int window_days) = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
