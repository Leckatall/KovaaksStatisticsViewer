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
    struct ScenarioId {
        std::string name;
        std::string hash;
        bool operator==(const ScenarioId& other) const {
            return hash == other.hash;
        }
        bool operator<(const ScenarioId& other) const {
            return hash < other.hash;
        }
    };

    struct ScenarioRunId {
        ScenarioId scenario_id;
        long long start_time;
        bool operator==(const ScenarioRunId& other) const {
            return scenario_id == other.scenario_id && start_time == other.start_time;
        }
        bool operator<(const ScenarioRunId& other) const {
            return scenario_id < other.scenario_id || (scenario_id == other.scenario_id && start_time < other.start_time);
        }
    };

    struct ScenarioPerf {
        ScenarioRunId run_id;
        float scenario_length;
        std::vector<ScenarioDataPoint> data;

        template<typename T>
        void add_data(float time, DataPointType type, T value);

        // void add_data(float time, DataPointType type, float value) const;

        void print() const {
            std::cout << "Scenario Name: " << run_id.scenario_id.name << std::endl;
            std::cout << "Scenario Hash: " << run_id.scenario_id.hash << std::endl;
            std::cout << "Start time: " << run_id.start_time << std::endl;
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
namespace std {
    template <>
    struct hash<ksv::domain::ScenarioId> {
        auto operator()(const ksv::domain::ScenarioId &scenario_id) const -> size_t {
            return hash<string>{}(scenario_id.hash);
        }
    };
    template <>
    struct hash<ksv::domain::ScenarioRunId> {
        auto operator()(const ksv::domain::ScenarioRunId &run_id) const -> size_t {
            return hash<ksv::domain::ScenarioId>{}(run_id.scenario_id) ^ hash<long long>{}(run_id.start_time);
        }
    };
}

#endif //KOVAAKSSTATSVIEWER_SCEN_PERF_H
