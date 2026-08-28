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
        // Bumped when profile.proto changes incompatibly so rejected stores are quarantined instead of silently mis-parsed.
        constexpr std::uint32_t kStoreVersion = 4;
        constexpr std::size_t kHeaderPrefixSize = 4096;

        std::optional<profile::Header> readProtoHeader(const std::filesystem::path& path) {
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
            profile::Header header;
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

    ProfileSerializer::ProfileSerializer(std::shared_ptr<IProfileMigrator> migrator)
        : m_migrator(std::move(migrator)) {}

    bool ProfileSerializer::save(const domain::UserProfile &profile, const std::filesystem::path &path) {
        const auto previous_header = readHeader(path);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        profile::Store proto;

        for (const auto &source : profile.sources().entries()) {
            auto *source_proto = proto.add_sources();
            source_proto->set_id(source.id.value);
            source_proto->set_parent_id(source.parent.value);
            source_proto->set_path(source.path);
        }

        const auto writeSourceRef = [](profile::SourceFileRef *ref, const domain::SourceFileRef &source) {
            ref->set_directory_id(source.directory.value);
            ref->set_filename(source.filename);
        };

        for (const auto &run: profile.getAllRunRecords()) {
            auto *run_proto = proto.add_runs();
            run_proto->mutable_scenario_id()->set_name(run.run_id.scenario_id.name);
            run_proto->mutable_scenario_id()->set_hash(run.run_id.scenario_id.hash);
            run_proto->set_start_time(run.run_id.start_time);
            run_proto->set_scenario_length(run.scenario_length);

            auto *totals = run_proto->mutable_totals();
            totals->set_score(run.stored_totals.score);
            totals->set_shots(run.stored_totals.shots);
            totals->set_hits(run.stored_totals.hits);
            totals->set_misses(run.stored_totals.misses);
            totals->set_kills(run.stored_totals.kills);

            if (run.sources.perf) writeSourceRef(run_proto->mutable_perf_source(), *run.sources.perf);
            if (run.sources.csv) writeSourceRef(run_proto->mutable_csv_source(), *run.sources.csv);

            if (run.performance) {
                auto *performance = run_proto->mutable_performance();
                for (const auto &point: run.performance->samples) {
                    auto *data_point = performance->add_samples();
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

            if (run.stats) {
                auto *stats = run_proto->mutable_stats();
                stats->set_sens_scale(run.stats->sens_scale);
                stats->set_horiz_sens(run.stats->horiz_sens);
                stats->set_vert_sens(run.stats->vert_sens);
                stats->set_dpi(run.stats->dpi);
                stats->set_fov(run.stats->fov);
                stats->set_fov_scale(run.stats->fov_scale);
                stats->set_resolution(run.stats->resolution);
                stats->set_resolution_scale(run.stats->resolution_scale);
                stats->set_avg_fps(run.stats->avg_fps);
            }
        }

        profile::File file;
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
            if (header->version < kStoreVersion && m_migrator) {
                auto migrated = m_migrator->migrate(path, header->version);
                if (!migrated) {
                    quarantineRejectedFile(path, "version-mismatch");
                    return application::ProfileLoadError::VersionMismatch;
                }
                // save() renames a temp file into place, so a failed rewrite leaves the legacy file intact.
                if (!save(*migrated, path)) return application::ProfileLoadError::VersionMismatch;
                return std::move(*migrated);
            }
            quarantineRejectedFile(path, "version-mismatch");
            return application::ProfileLoadError::VersionMismatch;
        }

        profile::File file;
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
            domain::Run run{};
            run.run_id.scenario_id.name = run_proto.scenario_id().name();
            run.run_id.scenario_id.hash = run_proto.scenario_id().hash();
            run.run_id.start_time = run_proto.start_time();
            run.scenario_length = run_proto.scenario_length();

            const auto &totals = run_proto.totals();
            run.stored_totals = {
                .score = totals.score(),
                .shots = totals.shots(),
                .hits = totals.hits(),
                .misses = totals.misses(),
                .kills = totals.kills(),
            };

            if (run_proto.has_perf_source()) {
                run.sources.perf = {{run_proto.perf_source().directory_id()}, run_proto.perf_source().filename()};
            }
            if (run_proto.has_csv_source()) {
                run.sources.csv = {{run_proto.csv_source().directory_id()}, run_proto.csv_source().filename()};
            }

            if (run_proto.has_performance()) {
                auto &performance = run.performance.emplace();
                for (const auto &data_point: run_proto.performance().samples()) {
                    auto point = domain::ScenarioDataPoint(data_point.time());
                    point.shots = data_point.shots();
                    point.hits = data_point.hits();
                    point.misses = data_point.misses();
                    point.dmg = data_point.dmg();
                    point.dmg_possible = data_point.dmg_possible();
                    point.score = data_point.score();
                    point.kills = data_point.kills();
                    performance.samples.push_back(point);
                }
            }

            if (run_proto.has_stats()) {
                const auto &stats = run_proto.stats();
                run.stats = domain::Stats{
                    .sens_scale = stats.sens_scale(),
                    .horiz_sens = stats.horiz_sens(),
                    .vert_sens = stats.vert_sens(),
                    .dpi = stats.dpi(),
                    .fov = stats.fov(),
                    .fov_scale = stats.fov_scale(),
                    .resolution = stats.resolution(),
                    .resolution_scale = stats.resolution_scale(),
                    .avg_fps = stats.avg_fps(),
                };
            }
            profile.addRun(run);
        }
        return profile;
    }
}
