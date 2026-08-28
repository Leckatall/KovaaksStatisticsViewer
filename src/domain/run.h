#ifndef KOVAAKSSTATSVIEWER_RUN_H
#define KOVAAKSSTATSVIEWER_RUN_H

#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "source_directory.h"

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
        int shots = 0;
        int hits = 0;
        int misses = 0;
        float dmg = 0.0F;
        float dmg_possible = 0.0F;
        float score = 0.0F;
        int kills = 0;

        explicit ScenarioDataPoint(const float time) : time(time) {}
    };

    struct ScenarioId {
        std::string name;
        std::string hash;

        bool operator==(const ScenarioId &other) const { return hash == other.hash; }
        bool operator<(const ScenarioId &other) const { return hash < other.hash; }
    };

    struct ScenarioRunId {
        ScenarioId scenario_id;
        long long start_time = 0;

        bool operator==(const ScenarioRunId &other) const {
            return scenario_id == other.scenario_id && start_time == other.start_time;
        }
        bool operator<(const ScenarioRunId &other) const {
            return scenario_id < other.scenario_id ||
                   (scenario_id == other.scenario_id && start_time < other.start_time);
        }
        [[nodiscard]] std::chrono::sys_seconds startSecond() const {
            using namespace std::chrono;
            return floor<seconds>(sys_time<milliseconds>{milliseconds{start_time}});
        }
        [[nodiscard]] std::chrono::sys_days startDay() const {
            using namespace std::chrono;
            return floor<days>(sys_time<milliseconds>{milliseconds{start_time}});
        }
        [[nodiscard]] std::string toString() const {
            const auto seconds = static_cast<std::time_t>(start_time / 1000);
            std::tm local_tm{};
#ifdef _WIN32
            if (localtime_s(&local_tm, &seconds) != 0) return scenario_id.name;
#else
            if (localtime_r(&seconds, &local_tm) == nullptr) return scenario_id.name;
#endif
            std::ostringstream value;
            value << scenario_id.name << " (" << std::put_time(&local_tm, "%Y-%m-%d, %H:%M:%S") << ")";
            return value.str();
        }
    };

    struct RunTotals {
        float score = 0.0F;
        int shots = 0;
        int hits = 0;
        int misses = 0;
        int kills = 0;

        [[nodiscard]] double accuracy() const {
            return shots == 0 ? 0.0 : static_cast<double>(hits) / shots;
        }

        bool operator==(const RunTotals &) const = default;
    };

    struct Performance {
        std::vector<ScenarioDataPoint> samples;

        template<typename T>
        void add_data(float time, DataPointType type, T value);

    private:
        [[nodiscard]] ScenarioDataPoint &get_data_point(float time);
    };

    struct Stats {
        std::string sens_scale;
        float horiz_sens = 0.0F;
        float vert_sens = 0.0F;
        int dpi = 0;
        float fov = 0.0F;
        std::string fov_scale;
        std::string resolution;
        float resolution_scale = 0.0F;
        float avg_fps = 0.0F;

        bool operator==(const Stats &) const = default;
    };

    struct RunSources {
        std::optional<SourceFileRef> perf;
        std::optional<SourceFileRef> csv;

        bool operator==(const RunSources &) const = default;
    };

    struct Run {
        ScenarioRunId run_id;
        float scenario_length = 0.0F;
        RunTotals stored_totals;
        RunSources sources;
        std::optional<Performance> performance;
        std::optional<Stats> stats;

        [[nodiscard]] const RunTotals &totals() const { return stored_totals; }
    };

    struct RunSummary {
        ScenarioRunId run_id;
        RunTotals totals;
    };
}

namespace std {
    template<>
    struct hash<ksv::domain::ScenarioId> {
        size_t operator()(const ksv::domain::ScenarioId &scenario_id) const {
            return hash<string>{}(scenario_id.hash);
        }
    };

    template<>
    struct hash<ksv::domain::ScenarioRunId> {
        size_t operator()(const ksv::domain::ScenarioRunId &run_id) const {
            return hash<ksv::domain::ScenarioId>{}(run_id.scenario_id) ^ hash<long long>{}(run_id.start_time);
        }
    };
}

#endif // KOVAAKSSTATSVIEWER_RUN_H
