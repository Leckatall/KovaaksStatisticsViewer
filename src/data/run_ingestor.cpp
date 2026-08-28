#include "run_ingestor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>

#include "formats/csv/run_filename.h"

namespace ksv::data {
    namespace {
        std::pair<std::string, std::string> keyFor(const std::string &root, const std::string &filename) {
            return {root, pairingStem(filename)};
        }
    }

    std::vector<RunFileGroup> pairRunFiles(const std::vector<application::PerfFile> &perfs,
                                            const std::vector<application::StatsFile> &csvs) {
        std::map<std::pair<std::string, std::string>, RunFileGroup> groups;
        for (const auto &perf : perfs) groups[keyFor(perf.root, perf.filename)].perf = perf;
        for (const auto &csv : csvs) groups[keyFor(csv.root, csv.filename)].csv = csv;
        std::vector<RunFileGroup> result;
        result.reserve(groups.size());
        for (auto &[_, group] : groups) result.push_back(std::move(group));
        return result;
    }

    RunIngestor::RunIngestor(std::shared_ptr<application::IFileService> file_service) : m_file_service(std::move(file_service)) {}

    std::optional<domain::Run> RunIngestor::buildRun(domain::UserProfile &profile, std::optional<application::PerfFile> perf,
                                                       std::optional<application::StatsFile> csv) const {
        if (!perf && !csv) return std::nullopt;
        std::optional<domain::Run> result;
        if (perf) {
            try { result = m_file_service->getPerfFromFile(perf->absolutePath()); }
            catch (const std::exception &error) {
                std::cerr << "Skipping " << perf->absolutePath() << ": " << error.what() << std::endl;
                if (!csv) return std::nullopt;
            }
            if (result) result->sources.perf = {{profile.ensureSource(perf->root, perf->subdir), perf->filename}};
        }
        if (csv) {
            const auto parsed = m_file_service->getStatsFromFile(csv->absolutePath());
            if (!parsed && !result) return std::nullopt;
            if (parsed) {
                if (result && (result->run_id.scenario_id.hash != parsed->scenario_id.hash ||
                    result->stored_totals.hits != parsed->totals.hits || result->stored_totals.misses != parsed->totals.misses ||
                    std::abs(result->stored_totals.score - parsed->totals.score) > 1e-4F)) {
                    std::cerr << "Perf/CSV totals or identity differ for " << csv->absolutePath() << std::endl;
                }
                if (!result) {
                    const auto timing = deriveCsvTiming(csv->filename, parsed->challenge_start);
                    if (!timing) return std::nullopt;
                    const auto paused_seconds = std::chrono::duration_cast<std::chrono::duration<float>>(
                        parsed->pause_duration).count();
                    const auto length = std::max(0.0F, timing->scenario_length - paused_seconds);
                    result = domain::Run{.run_id = {.scenario_id = parsed->scenario_id, .start_time = timing->start_time}, .scenario_length = length};
                }
                result->stored_totals = parsed->totals;
                result->stats = parsed->stats;
                result->sources.csv = {{profile.ensureSource(csv->root, csv->subdir), csv->filename}};
            }
        }
        return result;
    }

    std::optional<domain::Run> RunIngestor::buildLiveRun(domain::UserProfile &profile, const application::PerfFile &perf) const {
        const auto csv = statsSibling(perf);
        if (!std::filesystem::exists(csv.absolutePath())) std::cerr << "Missing stats sibling " << csv.absolutePath() << std::endl;
        return buildRun(profile, perf, std::filesystem::exists(csv.absolutePath()) ? std::optional{csv} : std::nullopt);
    }

    std::optional<domain::Run> RunIngestor::enrichStoredRun(domain::UserProfile &profile, domain::Run run) const {
        if (!run.sources.perf) return run;
        const auto perf_path = profile.sources().resolve(*run.sources.perf);
        if (!perf_path) return run;
        const auto path = std::filesystem::path(*perf_path);
        const application::PerfFile perf{
            .root = path.parent_path().parent_path().parent_path().generic_string(),
            .subdir = "FPSAimTrainer/performances",
            .filename = path.filename().string(),
        };
        const auto csv = statsSibling(perf);
        if (!std::filesystem::exists(csv.absolutePath())) return run;
        const auto parsed = m_file_service->getStatsFromFile(csv.absolutePath());
        if (!parsed) return run;
        run.stored_totals = parsed->totals;
        run.stats = parsed->stats;
        run.sources.csv = {{profile.ensureSource(csv.root, csv.subdir), csv.filename}};
        return run;
    }
}
