//
// Created by Lecka on 03/08/2026.
//

#include "profile_serializer.h"

#include <fstream>

namespace ksv::data {
    namespace {
        // Bumped when cache.proto changes incompatibly, ensuring old caches are regenerated instead of silently mis-parsed
        constexpr std::uint32_t kCacheVersion = 1;
    }

    void ProfileSerializer::save(const domain::UserProfile &profile, const std::filesystem::path &path) {
        cache::UserProfileCache proto;
        proto.set_version(kCacheVersion);
        proto.set_source_directory(profile.getSourceDirectory());

        for (const auto &perf: profile.getAllRunRecords()) {
            auto *run_proto = proto.add_runs();
            run_proto->mutable_scenario_id()->set_name(perf.run_id.scenario_id.name);
            run_proto->mutable_scenario_id()->set_hash(perf.run_id.scenario_id.hash);
            run_proto->set_start_time(perf.run_id.start_time);
            run_proto->set_scenario_length(perf.scenario_length);
            run_proto->set_source_file(perf.source_file);

            for (const auto &point: perf.data) {
                auto *data_point = run_proto->add_data();
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

        std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
        proto.SerializeToOstream(&output);
    }

    std::optional<domain::UserProfile> ProfileSerializer::load(const std::filesystem::path &path) {
        if (!std::filesystem::exists(path)) return std::nullopt;

        cache::UserProfileCache proto;
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!proto.ParseFromIstream(&input)) return std::nullopt;

        // Reject incompatible cache schema to force regeneration from .perf files instead of silent mis-parsing
        if (proto.version() != kCacheVersion) return std::nullopt;

        domain::UserProfile profile{proto.source_directory()};
        for (const auto &run_proto: proto.runs()) {
            domain::ScenarioPerf perf{};
            perf.run_id.scenario_id.name = run_proto.scenario_id().name();
            perf.run_id.scenario_id.hash = run_proto.scenario_id().hash();
            perf.run_id.start_time = run_proto.start_time();
            perf.scenario_length = run_proto.scenario_length();
            perf.source_file = run_proto.source_file();

            for (const auto &data_point: run_proto.data()) {
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
        return profile;
    }
}
