//
// Created by Lecka on 29/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SCEN_PERF_H
#define KOVAAKSSTATSVIEWER_SCEN_PERF_H
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

namespace ksv::domain {
    enum DataPointType {
        SHOTS,
        HITS,
        MISSES,
        DMG,
        DMG_POSSIBLE,
        SCORE,
        KILLS
    };

    struct ScenarioDataPoint {
        float time;
        int shots;
        int hits;
        int misses;
        float dmg;
        float dmg_possible;
        float score;
        int kills;
    };

    struct ScenarioPerf {
        std::string scenario_name;
        long long start_time;
        float scenario_length;
        std::vector<ScenarioDataPoint> data;

        template<typename T>
        void add_data(float time, DataPointType type, T value);

        // void add_data(float time, DataPointType type, float value) const;

        void print() const {
            std::cout << "Scenario: " << scenario_name << std::endl;
            std::cout << "Start time: " << start_time << std::endl;
            std::cout << "Duration: " << scenario_length << std::endl;
            std::cout << "Data:" << std::endl;
            for (const auto &point: data) {
                std::cout << point.time << " " << point.shots << " " << point.hits << " " << point.misses << " " << point.dmg << " " << point.dmg_possible << " " << point.score << " " << point.kills << std::endl;
            }
        }

    private:
        [[nodiscard]] ScenarioDataPoint& get_data_point(float time);
    };


}


#endif //KOVAAKSSTATSVIEWER_SCEN_PERF_H
