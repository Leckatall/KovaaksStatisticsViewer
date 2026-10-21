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

    IngestResult RunIngestor::ingestRun(domain::UserProfile &profile, std::optional<application::PerfFile> perf,
                                       std::optional<application::StatsFile> csv) const {
        return ingestSibling(profile, {}, std::move(perf), std::move(csv));
    }

    IngestResult RunIngestor::ingestSibling(domain::UserProfile &profile, domain::Run existing,
            std::optional<application::PerfFile> perf, std::optional<application::StatsFile> csv) const {
        IngestResult result;
        if (!existing.run_id.scenario_id.hash.empty()) result.run = std::move(existing);
        if (perf) {
            try {
                auto decoded = m_file_service->getPerfFromFile(perf->absolutePath());
                if (decoded.run_id.scenario_id.hash.empty() || !std::isfinite(decoded.scenario_length) || decoded.scenario_length < 0) {
                    result.issues.push_back({perf->absolutePath(), "Performance metadata is incomplete or invalid", true});
                } else if (result.run && !(decoded.run_id.scenario_id == result.run->run_id.scenario_id)) {
                    result.issues.push_back({perf->absolutePath(), "Perf/CSV identity differs", false});
                } else {
                    if (result.run && result.run->sources.csv) {
                        decoded.sources.csv = result.run->sources.csv;
                        decoded.stats = result.run->stats;
                        decoded.stored_totals = result.run->stored_totals;
                    }
                    decoded.sources.perf = {{profile.ensureSource(perf->root, perf->subdir), perf->filename}};
                    result.run = std::move(decoded);
                }
            } catch (const std::exception &error) {
                result.issues.push_back({perf->absolutePath(), error.what(), true});
            }
        }
        if (csv) {
            try {
                const auto parsed = m_file_service->getStatsFromFile(csv->absolutePath());
                if (!parsed) {
                    result.issues.push_back({csv->absolutePath(), "Stats CSV is incomplete or invalid", true});
                } else if (result.run && !(result.run->run_id.scenario_id == parsed->scenario_id)) {
                    result.issues.push_back({csv->absolutePath(), "Perf/CSV totals or identity differ", false});
                } else {
                    if (!result.run) {
                        const auto timing = deriveCsvTiming(csv->filename, parsed->challenge_start);
                        if (!timing) {
                            result.issues.push_back({csv->absolutePath(), "Cannot derive run timing from CSV filename", false});
                            return result;
                        }
                        const auto paused = std::chrono::duration<float>(parsed->pause_duration).count();
                        result.run = domain::Run{.run_id = {parsed->scenario_id, timing->start_time},
                            .scenario_length = std::max(0.0F, timing->scenario_length - paused)};
                    }
                    result.run->stored_totals = parsed->totals;
                    result.run->stats = parsed->stats;
                    result.run->sources.csv = {{profile.ensureSource(csv->root, csv->subdir), csv->filename}};
                }
            } catch (const std::exception &error) {
                result.issues.push_back({csv->absolutePath(), error.what(), true});
            }
        }
        return result;
    }

    std::optional<domain::Run> RunIngestor::buildRun(domain::UserProfile &profile, std::optional<application::PerfFile> perf,
                                                       std::optional<application::StatsFile> csv) const {
        auto result = ingestRun(profile, std::move(perf), std::move(csv));
        for (const auto &issue : result.issues)
            std::cerr << "Skipping " << issue.path << ": " << issue.message << std::endl;
        return std::move(result.run);
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
        if (run.run_id.scenario_id.hash != parsed->scenario_id.hash) {
            std::cerr << "Perf/CSV totals or identity differ for " << csv.absolutePath() << std::endl;
            return run;
        }
        run.stored_totals = parsed->totals;
        run.stats = parsed->stats;
        run.sources.csv = {{profile.ensureSource(csv.root, csv.subdir), csv.filename}};
        return run;
    }
}
