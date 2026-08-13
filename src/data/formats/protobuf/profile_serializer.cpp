//
// Created by Lecka on 03/08/2026.
//

#include "profile_serializer.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

namespace ksv::data {
    namespace {
        // Bumped when store.proto changes incompatibly so rejected stores are quarantined instead of silently mis-parsed.
        constexpr std::uint32_t kStoreVersion = 1;

        std::optional<std::uint64_t> contentDigest(const std::filesystem::path &path) {
            constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
            constexpr std::uint64_t prime = 1099511628211ULL;

            std::ifstream input(path, std::ios::in | std::ios::binary);
            if (!input) return std::nullopt;

            std::uint64_t digest = offset_basis;
            char buffer[8192];
            while (input) {
                input.read(buffer, sizeof(buffer));
                for (std::streamsize i = 0; i < input.gcount(); ++i) {
                    digest ^= static_cast<unsigned char>(buffer[i]);
                    digest *= prime;
                }
            }
            if (input.bad()) return std::nullopt;
            return digest;
        }

        std::string utcTimestamp() {
            using namespace std::chrono;
            const auto now = system_clock::now();
            const auto seconds = system_clock::to_time_t(now);
            std::tm utc_tm{};
#ifdef _WIN32
            const bool converted = gmtime_s(&utc_tm, &seconds) == 0;
#else
            const bool converted = gmtime_r(&seconds, &utc_tm) != nullptr;
#endif
            if (converted) {
                std::ostringstream formatted;
                formatted << std::put_time(&utc_tm, "%Y%m%dT%H%M%SZ");
                if (formatted) return formatted.str();
            }
            return std::to_string(duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
        }

        void quarantineRejectedFile(const std::filesystem::path &path, const std::string_view reason) {
            std::ostringstream suffix;
            suffix << '_' << reason << '_' << utcTimestamp();
            if (const auto digest = contentDigest(path)) {
                suffix << '_' << std::hex << std::setw(16) << std::setfill('0') << *digest;
            }

            auto quarantine_name = path.stem();
            quarantine_name += suffix.str();
            quarantine_name += path.extension();

            std::error_code error;
            std::filesystem::rename(path, path.parent_path() / quarantine_name, error);
        }
    }

    void ProfileSerializer::save(const domain::UserProfile &profile, const std::filesystem::path &path) {
        store::UserProfileStore proto;
        proto.set_version(kStoreVersion);
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

        // TODO(2026-08-13): Replace the live-path truncation once save writes a temporary file and renames it atomically.
        std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
        proto.SerializeToOstream(&output);
    }

    std::optional<domain::UserProfile> ProfileSerializer::load(const std::filesystem::path &path) {
        if (!std::filesystem::exists(path)) return std::nullopt;

        store::UserProfileStore proto;
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!proto.ParseFromIstream(&input)) {
            input.close();
            quarantineRejectedFile(path, "unparseable");
            // TODO(2026-08-13): Widen IProfileSerializer::load's result once callers distinguish rejection from absence.
            return std::nullopt;
        }
        input.close();

        // The field layout cannot be interpreted safely under an incompatible version.
        if (proto.version() != kStoreVersion) {
            quarantineRejectedFile(path, "version-mismatch");
            return std::nullopt;
        }

        domain::UserProfile profile{proto.source_directory()};
        for (const auto &run_proto: proto.runs()) {
            domain::ScenarioPerf perf{};
            perf.run_id.scenario_id.name = run_proto.scenario_id().name();
            perf.run_id.scenario_id.hash = run_proto.scenario_id().hash();
            perf.run_id.start_time = run_proto.start_time();
            perf.scenario_length = run_proto.scenario_length();
            perf.source_file = run_proto.source_file();

            for (const auto &data_point: run_proto.data()) {
                auto point = domain::ScenarioDataPoint(data_point.time());
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
