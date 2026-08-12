//
// Created by Lecka on 29/07/2026.
//

#include "scenario_perf.h"

namespace ksv::domain {

    template<typename T>
    void ScenarioPerf::add_data(const float time, const DataPointType type, T value) {
        ScenarioDataPoint& point = get_data_point(time);

        switch (type) {
            case SHOTS:
            case HITS:
            case MISSES:
            case KILLS:{
                if (!std::is_integral_v<T>) throw std::invalid_argument("Value is not an integer");
                const int v = static_cast<int>(value);
                if (type == SHOTS) point.shots += v;
                else if (type == HITS) point.hits += v;
                else if (type == MISSES) point.misses += v;
                else point.kills += v;
                break;
            }

            case DMG:
            case DMG_POSSIBLE:
            case SCORE: {
                if (!std::is_floating_point_v<T>) throw std::invalid_argument("Value is not a floating point");
                const float v = static_cast<float>(value);
                if (type == DMG) point.dmg += v;
                else if (type == DMG_POSSIBLE) point.dmg_possible += v;
                else point.score += v;
                break;
            }

            default:
                throw std::invalid_argument("Unknown DataPointType");
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

