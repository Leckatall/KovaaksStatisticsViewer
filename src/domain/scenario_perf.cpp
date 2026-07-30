//
// Created by Lecka on 29/07/2026.
//

#include "scenario_perf.h"

namespace ksv::domain {

    template<typename T>
    void ScenarioPerf::add_data(const float time, const DataPointType type, T value) {
        std::cout << "Adding data: " << time << " " << type << " " << value << std::endl;
        ScenarioDataPoint& point = get_data_point(time);

        switch (type) {
            case SHOTS:
            case HITS:
            case MISSES:
            case KILLS:{
                if (!std::is_integral_v<T>) throw std::invalid_argument("Value is not an integer");
                const int v = static_cast<int>(value);
                if (type == SHOTS) point.shots = v;
                else if (type == HITS) point.hits = v;
                else point.misses = v;
                break;
            }

            case DMG:
            case DMG_POSSIBLE:
            case SCORE: {
                if (!std::is_floating_point_v<T>) throw std::invalid_argument("Value is not a floating point");
                const float v = static_cast<float>(value);
                if (type == DMG) point.dmg = v;
                else if (type == DMG_POSSIBLE) point.dmg_possible = v;
                else point.score = v;
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
        auto point = ScenarioDataPoint{.time = time};
        data.push_back(point);
        return data.back();
    }
}

