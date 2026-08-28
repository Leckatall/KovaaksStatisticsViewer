#include "profile_v3_migrator.h"

#include <fstream>
#include <utility>
#include <vector>

#include "formats/protobuf/migration/profile_v3.pb.h"

namespace ksv::data {
    namespace {
        constexpr std::uint32_t kV3Version = 3;

        domain::RunTotals sumSamples(const google::protobuf::RepeatedPtrField<profile_v3::ScenarioDataPoint> &data) {
            domain::RunTotals totals;
            for (const auto &point: data) {
                totals.score += point.score();
                totals.shots += point.shots();
                totals.hits += point.hits();
                totals.misses += point.misses();
                totals.kills += point.kills();
            }
            return totals;
        }
    }

    ProfileV3Migrator::ProfileV3Migrator(std::shared_ptr<IRunIngestor> ingestor)
        : m_ingestor(std::move(ingestor)) {}

    std::optional<domain::UserProfile> ProfileV3Migrator::migrate(
        const std::filesystem::path &path, const std::uint32_t from_version) const {
        if (from_version != kV3Version) return std::nullopt;

        profile_v3::ProfileStoreFile file;
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!file.ParseFromIstream(&input)) return std::nullopt;

        const auto &store = file.store();

        std::vector<domain::SourceDirectory> sources;
        sources.reserve(store.sources_size());
        for (const auto &source: store.sources()) {
            sources.push_back({{source.id()}, {source.parent_id()}, source.path()});
        }
        domain::UserProfile profile{domain::SourceRegistry{std::move(sources)}};

        for (const auto &old: store.runs()) {
            domain::Run run{
                .run_id = {{old.scenario_id().name(), old.scenario_id().hash()}, old.start_time()},
                .scenario_length = old.scenario_length(),
                .stored_totals = sumSamples(old.data()),
                .sources = {.perf = domain::SourceFileRef{{old.source_directory_id()}, old.source_filename()}},
                .performance = domain::Performance{},
            };
            for (const auto &point: old.data()) {
                auto sample = domain::ScenarioDataPoint(point.time());
                sample.shots = point.shots();
                sample.hits = point.hits();
                sample.misses = point.misses();
                sample.dmg = point.dmg();
                sample.dmg_possible = point.dmg_possible();
                sample.score = point.score();
                sample.kills = point.kills();
                run.performance->samples.push_back(sample);
            }

            auto enriched = m_ingestor->enrichStoredRun(profile, std::move(run));
            if (enriched) profile.addRun(*enriched);
        }
        return profile;
    }
}
