//
// Created by Lecka on 03/08/2026.
//

#include "profile_serializer.h"

#include <fstream>

namespace ksv::data {
    void ProfileSerializer::save(const domain::UserProfile &profile, const std::filesystem::path &path) {
        cache::UserProfileCache proto;
        for (const auto &[scenario_id, runs]: profile.getAllRuns()) {
            auto *scenario_runs = proto.add_scenarios();
            scenario_runs->mutable_id()->set_name(scenario_id.name);
            scenario_runs->mutable_id()->set_hash(scenario_id.hash);

            for (const auto &perf: runs) {
                auto *perf_proto = scenario_runs->add_perfs();
                perf_proto->mutable_scenario_id()->set_name(perf.run_id.scenario_id.name);
                perf_proto->mutable_scenario_id()->set_hash(perf.run_id.scenario_id.hash);
                perf_proto->set_start_time(perf.run_id.start_time);
                perf_proto->set_scenario_length(perf.scenario_length);

                for (const auto &point: perf.data) {
                    auto *data_point = perf_proto->add_data();
                    data_point->set_time(point.time);
                    data_point->set_shots(point.shots);
                    data_point->set_hits(point.hits);
                    data_point->set_misses(point.misses);
                    data_point->set_dmg(point.dmg);
                    data_point->set_dmg_possible(point.dmg_possible);
                    data_point->set_score(point.score);
                    data_point->set_kills(point.kills);
                }
            }
        }

        std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
        proto.SerializeToOstream(&output);
    }

    std::optional<domain::UserProfile> ProfileSerializer::load(const std::filesystem::path &path) {
        if (!std::filesystem::exists(path)) return std::nullopt;

        cache::UserProfileCache proto;
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!proto.ParseFromIstream(&input)) return std::nullopt;

        domain::UserProfile profile{"default"};
        for (const auto &scenario_runs: proto.scenarios()) {
            for (const auto &perf_proto: scenario_runs.perfs()) {
                domain::ScenarioPerf perf{};
                perf.run_id.scenario_id.name = perf_proto.scenario_id().name();
                perf.run_id.scenario_id.hash = perf_proto.scenario_id().hash();
                perf.run_id.start_time = perf_proto.start_time();
                perf.scenario_length = perf_proto.scenario_length();

                for (const auto &data_point: perf_proto.data()) {
                    domain::ScenarioDataPoint point{};
                    point.time = data_point.time();
                    point.shots = data_point.shots();
                    point.hits = data_point.hits();
                    point.misses = data_point.misses();
                    point.dmg = data_point.dmg();
                    point.dmg_possible = data_point.dmg_possible();
                    point.score = data_point.score();
                    point.kills = data_point.kills();
                    perf.data.push_back(point);
                }
                profile.addScenarioPerf(perf);
            }
        }
        return profile;
    }
}
