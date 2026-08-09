//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_USE_CASE_H
#define KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_USE_CASE_H
#include <memory>
#include <utility>
#include <vector>

#include "i_playtime_graph_use_case.h"
#include "data/interfaces/i_profile_service.h"

namespace ksv::application {
    class PlaytimeGraphUseCase : public IPlaytimeGraphUseCase {
    public:
        explicit PlaytimeGraphUseCase(std::shared_ptr<IProfileService> profile_service)
            : m_profile_service(std::move(profile_service)) {}

        std::vector<std::pair<long long, double>> get_rolling_playtime(const int window_days) override {
            const auto series = m_profile_service->getRollingTimeAverage(window_days);
            std::vector<std::pair<long long, double>> result;
            result.reserve(series.size());
            for (const auto &[day, avg_seconds]: series) {
                // sys_days counts days since Unix epoch (1970-01-01); used as graph X value
                result.emplace_back(static_cast<long long>(day.time_since_epoch().count()), avg_seconds);
            }
            return result;
        }

    private:
        std::shared_ptr<IProfileService> m_profile_service;
    };
}

#endif //KOVAAKSSTATSVIEWER_PLAYTIME_GRAPH_USE_CASE_H
