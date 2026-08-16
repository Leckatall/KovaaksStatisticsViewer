//
// Created by Lecka on 29/07/2026.
//

#include "scenario_perf.h"

namespace ksv::domain {

    template<typename T>
    void ScenarioPerf::add_data(const float time, const DataPointType type, T value) {
        switch (type) {
            case SHOTS:
            case HITS:
            case MISSES:
            case KILLS:
                if (!std::is_integral_v<T>) throw std::invalid_argument("Value is not an integer");
                break;

            case DMG:
            case DMG_POSSIBLE:
            case SCORE:
                if (!std::is_floating_point_v<T>) throw std::invalid_argument("Value is not a floating point");
                break;

            default:
                throw std::invalid_argument("Unknown DataPointType");
        }

        ScenarioDataPoint& point = get_data_point(time);
        switch (type) {
            case SHOTS: point.shots += static_cast<int>(value); break;
            case HITS: point.hits += static_cast<int>(value); break;
            case MISSES: point.misses += static_cast<int>(value); break;
            case KILLS: point.kills += static_cast<int>(value); break;
            case DMG: point.dmg += static_cast<float>(value); break;
            case DMG_POSSIBLE: point.dmg_possible += static_cast<float>(value); break;
            case SCORE: point.score += static_cast<float>(value); break;
        }
    }

    ScenarioDataPoint& ScenarioPerf::get_data_point(const float time) {
        for (auto &point: data) {
            if (point.time == time) {
                return point;
            }
        }
        const auto point = ScenarioDataPoint(time);
        data.push_back(point);
        return data.back();
    }

    template void ScenarioPerf::add_data<int>(float time, DataPointType type, int value);
    template void ScenarioPerf::add_data<unsigned int>(float time, DataPointType type, unsigned int value);
    template void ScenarioPerf::add_data<float>(float time, DataPointType type, float value);
}

