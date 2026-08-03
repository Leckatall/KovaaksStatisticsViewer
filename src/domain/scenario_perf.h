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

    // Aggregate totals for a finished run. scenario_time comes straight from
    // ScenarioPerf::scenario_length; every other field is the sum of that
    // stat across all ScenarioDataPoints (the per-tick values are not
    // cumulative, so summing is what yields the run's actual totals).
    struct ScenarioCompletionData {
        float scenario_time = 0.0F;
        int shots = 0;
        int hits = 0;
        int misses = 0;
        float dmg = 0.0F;
        float dmg_possible = 0.0F;
        float score = 0.0F;
        int kills = 0;
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
        //TODO: implement converting this to a string of the form scenario_id.name (start_date, start_time)
        // This string can then be used in the ui to show what scenario is being displayed
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

        // The score reported at the end of a run (data points are chronological).
        [[nodiscard]] float getFinalScore() const {
            return data.empty() ? 0.0F : data.back().score;
        }

        [[nodiscard]] ScenarioCompletionData getCompletionData() const {
            ScenarioCompletionData completion;
            completion.scenario_time = scenario_length;
            for (const auto &point: data) {
                completion.shots += point.shots;
                completion.hits += point.hits;
                completion.misses += point.misses;
                completion.dmg += point.dmg;
                completion.dmg_possible += point.dmg_possible;
                completion.score += point.score;
                completion.kills += point.kills;
            }
            return completion;
        }

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
