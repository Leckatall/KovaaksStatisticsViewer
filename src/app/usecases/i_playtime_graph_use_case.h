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

        virtual std::vector<std::pair<long long, double>> get_rolling_playtime(int window_days) = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_PLAYTIME_GRAPH_USE_CASE_H
