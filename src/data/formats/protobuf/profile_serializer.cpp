//
// Created by Lecka on 03/08/2026.
//

#include "profile_serializer.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite.h>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

namespace ksv::data {
    namespace {
        // Bumped when store.proto changes incompatibly so rejected stores are quarantined instead of silently mis-parsed.
        constexpr std::uint32_t kStoreVersion = 3;
        constexpr std::size_t kHeaderPrefixSize = 4096;

        std::optional<store::StoreHeader> readProtoHeader(const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::in | std::ios::binary);
            if (!input) return std::nullopt;

            std::array<std::uint8_t, kHeaderPrefixSize> buffer{};
            input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            if (input.bad() || input.gcount() <= 0) return std::nullopt;

            google::protobuf::io::CodedInputStream coded_input(
                buffer.data(), static_cast<int>(input.gcount()));
            const auto header_tag = google::protobuf::internal::WireFormatLite::MakeTag(
                1, google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
            if (coded_input.ReadTag() != header_tag) return std::nullopt;

            int header_size = 0;
            if (!coded_input.ReadVarintSizeAsInt(&header_size)) return std::nullopt;
            const auto limit = coded_input.PushLimit(header_size);
            store::StoreHeader header;
            const bool parsed = header.MergeFromCodedStream(&coded_input) && coded_input.ConsumedEntireMessage();
            coded_input.PopLimit(limit);
            if (!parsed) return std::nullopt;
            return header;
        }

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

    bool ProfileSerializer::save(const domain::UserProfile &profile, const std::filesystem::path &path) {
        const auto previous_header = readHeader(path);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        store::UserProfileStore proto;

        for (const auto &source : profile.sources().entries()) {
            auto *source_proto = proto.add_sources();
            source_proto->set_id(source.id.value);
            source_proto->set_parent_id(source.parent.value);
            source_proto->set_path(source.path);
        }

        for (const auto &perf: profile.getAllRunRecords()) {
            auto *run_proto = proto.add_runs();
            run_proto->mutable_scenario_id()->set_name(perf.run_id.scenario_id.name);
            run_proto->mutable_scenario_id()->set_hash(perf.run_id.scenario_id.hash);
            run_proto->set_start_time(perf.run_id.start_time);
            run_proto->set_scenario_length(perf.scenario_length);
            run_proto->set_source_directory_id(perf.source.directory.value);
            run_proto->set_source_filename(perf.source.filename);

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

        store::ProfileStoreFile file;
        auto* header = file.mutable_header();
        header->set_version(kStoreVersion);
        header->set_created_at(previous_header && previous_header->created_at != 0
                                   ? previous_header->created_at
                                   : now);
        header->set_name(previous_header && !previous_header->name.empty() ? previous_header->name : "default");
        *file.mutable_store() = std::move(proto);

        auto temp_path = path;
        temp_path += ".tmp";
        const auto discard_temp = [&temp_path] {
            std::error_code discarded;
            std::filesystem::remove(temp_path, discarded);
        };

        std::ofstream output(temp_path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output || !file.SerializeToOstream(&output)) {
            output.close();
            discard_temp();
            return false;
        }
        output.flush();
        if (!output) {
            output.close();
            discard_temp();
            return false;
        }
        output.close();
        if (!output) {
            discard_temp();
            return false;
        }

        std::error_code error;
        std::filesystem::rename(temp_path, path, error);
        if (error) {
            discard_temp();
            return false;
        }
        return true;
    }

    std::optional<application::ProfileStoreHeader> ProfileSerializer::readHeader(
        const std::filesystem::path& path) const {
        const auto header = readProtoHeader(path);
        if (!header) return std::nullopt;
        return application::ProfileStoreHeader{
            .version = header->version(),
            .created_at = header->created_at(),
            .name = header->name(),
        };
    }

    application::ProfileLoadResult ProfileSerializer::load(const std::filesystem::path &path) {
        if (!std::filesystem::exists(path)) return application::ProfileLoadError::NotFound;

        const auto header = readHeader(path);
        if (!header) {
            quarantineRejectedFile(path, "unparseable");
            return application::ProfileLoadError::Unparseable;
        }
        if (header->version != kStoreVersion) {
            quarantineRejectedFile(path, "version-mismatch");
            return application::ProfileLoadError::VersionMismatch;
        }

        store::ProfileStoreFile file;
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!file.ParseFromIstream(&input)) {
            input.close();
            quarantineRejectedFile(path, "unparseable");
            return application::ProfileLoadError::Unparseable;
        }
        input.close();

        const auto& proto = file.store();

        std::vector<domain::SourceDirectory> sources;
        sources.reserve(proto.sources_size());
        for (const auto &source_proto : proto.sources()) {
            sources.push_back({
                {source_proto.id()},
                {source_proto.parent_id()},
                source_proto.path()
            });
        }
        domain::UserProfile profile{domain::SourceRegistry{std::move(sources)}};
        for (const auto &run_proto: proto.runs()) {
            domain::ScenarioPerf perf{};
            perf.run_id.scenario_id.name = run_proto.scenario_id().name();
            perf.run_id.scenario_id.hash = run_proto.scenario_id().hash();
            perf.run_id.start_time = run_proto.start_time();
            perf.scenario_length = run_proto.scenario_length();
            perf.source.directory = {run_proto.source_directory_id()};
            perf.source.filename = run_proto.source_filename();

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
