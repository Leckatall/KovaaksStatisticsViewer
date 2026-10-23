#include "profile_importer.h"
#include "run_ingestor.h"
#include "source_path_key.h"
#include "formats/csv/run_filename.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <tuple>

namespace ksv::data {
    namespace {
        std::string groupKey(const std::string &root, const std::string &filename) {
            return sourcePathKey(root) + '/' + sourcePathKey(pairingStem(filename));
        }
        std::optional<std::string> rootOf(const domain::UserProfile &profile, const domain::SourceFileRef &ref) {
            const auto &entries = profile.sources().entries();
            const auto directory = std::ranges::find(entries, ref.directory, &domain::SourceDirectory::id);
            if (directory == entries.end()) return std::nullopt;
            return profile.sources().resolve(directory->parent.value == 0 ? directory->id : directory->parent);
        }
        bool samePerformance(const std::optional<domain::Performance> &a, const std::optional<domain::Performance> &b) {
            if (a.has_value() != b.has_value()) return false;
            if (!a) return true;
            return std::ranges::equal(a->samples, b->samples, [](const auto &x, const auto &y) {
                return std::tie(x.time, x.shots, x.hits, x.misses, x.dmg, x.dmg_possible, x.score, x.kills) ==
                    std::tie(y.time, y.shots, y.hits, y.misses, y.dmg, y.dmg_possible, y.score, y.kills);
            });
        }
        // Damage joined RunTotals after profiles were saved, so an absent value on either side is not a difference.
        bool sameTotalsForImport(const domain::RunTotals &a, const domain::RunTotals &b) {
            auto lhs = a;
            auto rhs = b;
            if (!lhs.damage || !rhs.damage) lhs.damage = rhs.damage = std::nullopt;
            return lhs == rhs;
        }
        struct ScannedFile {
            std::string root;
            std::string subdir;
            std::string filename;
            std::int64_t size = -1;
            std::int64_t modified_ms = -1;
        };
        using FailureFingerprints = std::map<std::string, std::tuple<std::int64_t, std::int64_t, std::string>>;
        FailureFingerprints fingerprintsOf(const domain::UserProfile &profile) {
            FailureFingerprints fingerprints;
            for (const auto &failed : profile.failedSources())
                if (const auto path = profile.sources().resolve(failed.file))
                    fingerprints.emplace(sourcePathKey(*path), std::tuple{failed.size, failed.modified_ms, failed.message});
            return fingerprints;
        }
    }

    ProfileImporter::ProfileImporter(std::shared_ptr<application::IFileService> files, std::shared_ptr<IRunIngestor> ingestor)
        : m_files(std::move(files)), m_ingestor(std::move(ingestor)) {}

    application::ProfileImportResult ProfileImporter::run(const application::ProfileImportRequest &request,
            const application::ProfileImportProgress &progress) const {
        using namespace application;
        ProfileImportResult result;
        result.token = request.token;
        try {
            if (progress) progress(ProfileImportPhase::Scanning, 0, 0);
            auto scan = m_files->scanSources(request.roots);
            result.summary.issues = std::move(scan.issues);
            std::unordered_map<std::string, domain::ScenarioRunId> known_paths;
            std::unordered_map<std::string, domain::ScenarioRunId> known_groups;
            if (request.mode == ProfileImportMode::Refresh && request.baseline) {
                for (const auto &run : request.baseline->getAllRunRecords()) {
                    for (const auto &ref : {run.sources.perf, run.sources.csv}) {
                        if (!ref) continue;
                        const auto path = request.baseline->sources().resolve(*ref);
                        const auto root = rootOf(*request.baseline, *ref);
                        if (!path || !root) continue;
                        known_paths[sourcePathKey(*path)] = run.run_id;
                        known_groups[groupKey(*root, ref->filename)] = run.run_id;
                    }
                }
                for (const auto &alias : request.known_aliases) {
                    known_paths[sourcePathKey(alias.path)] = alias.run_id;
                    known_groups[groupKey(alias.root, alias.filename)] = alias.run_id;
                }
            }
            const auto baselineCopy = [&request] {
                return request.mode == ProfileImportMode::Refresh && request.baseline
                    ? std::make_shared<domain::UserProfile>(*request.baseline)
                    : std::make_shared<domain::UserProfile>();
            };
            std::unordered_map<std::string, domain::FailedSource> previous;
            if (request.mode == ProfileImportMode::Refresh && request.baseline) {
                for (const auto &failed : request.baseline->failedSources())
                    if (const auto path = request.baseline->sources().resolve(failed.file))
                        previous.emplace(sourcePathKey(*path), failed);
            }
            std::unordered_map<std::string, ScannedFile> scanned;
            std::unordered_map<std::string, domain::FailedSource> kept;
            std::map<std::string, RunFileGroup> groups;
            const auto admit = [&](const auto &file) {
                const auto path_key = sourcePathKey(file.absolutePath());
                scanned[path_key] = {file.root, file.subdir, file.filename, file.size, file.modified_ms};
                if (known_paths.contains(path_key)) return false;
                if (request.skip_unchanged_failures) {
                    const auto found = previous.find(path_key);
                    if (found != previous.end() && found->second.size == file.size &&
                        found->second.modified_ms == file.modified_ms) {
                        kept.emplace(path_key, found->second);
                        result.summary.issues.push_back({file.absolutePath(), found->second.message, false, true});
                        return false;
                    }
                }
                return true;
            };
            for (const auto &perf : scan.perfs)
                if (admit(perf)) groups[groupKey(perf.root, perf.filename)].perf = perf;
            for (const auto &csv : scan.csvs)
                if (admit(csv)) groups[groupKey(csv.root, csv.filename)].csv = csv;
            std::shared_ptr<domain::UserProfile> candidate;
            if (!groups.empty() || request.mode == ProfileImportMode::Rebuild) candidate = baselineCopy();
            if (request.mode == ProfileImportMode::Rebuild) result.changed = true;
            std::set<domain::ScenarioRunId> added;
            std::set<domain::ScenarioRunId> enriched;
            std::size_t done = 0;
            for (const auto &[key, group] : groups) {
                std::optional<domain::Run> existing;
                if (const auto found = known_groups.find(key); found != known_groups.end())
                    existing = candidate->getCurrentRun(found->second);
                auto ingested = existing
                    ? m_ingestor->ingestSibling(*candidate, *existing, group.perf, group.csv)
                    : m_ingestor->ingestRun(*candidate, group.perf, group.csv);
                result.summary.issues.insert(result.summary.issues.end(), ingested.issues.begin(), ingested.issues.end());
                if (ingested.run) {
                    auto &run = *ingested.run;
                    const auto same_id = candidate->getCurrentRun(run.run_id);
                    const auto previous_id = existing ? std::optional{existing->run_id} : std::nullopt;
                    bool conflicting = false;
                    if (!existing && same_id) {
                        conflicting = (run.sources.csv && same_id->sources.csv &&
                            (!sameTotalsForImport(run.stored_totals, same_id->stored_totals) || run.stats != same_id->stats)) ||
                            (run.sources.perf && same_id->sources.perf &&
                             (run.scenario_length != same_id->scenario_length || !samePerformance(run.performance, same_id->performance)));
                    }
                    const auto merge = conflicting ? domain::ImportMerge::Conflict
                        : candidate->mergeImportedRun(run, previous_id);
                    if (merge == domain::ImportMerge::Conflict) {
                        // Both halves are reported, so both are recorded: a recorded perf alone would let
                        // its csv merge by itself into the run the pair was rejected against.
                        if (group.perf) result.summary.issues.push_back({group.perf->absolutePath(),
                            "Source conflicts with an existing run", false});
                        if (group.csv) result.summary.issues.push_back({group.csv->absolutePath(),
                            "Source conflicts with an existing run", false});
                    } else {
                        if (merge == domain::ImportMerge::Added || merge == domain::ImportMerge::Enriched) result.changed = true;
                        if (existing && !(existing->run_id == run.run_id)) {
                            if (added.erase(existing->run_id)) added.insert(run.run_id);
                            if (enriched.erase(existing->run_id)) enriched.insert(run.run_id);
                            result.rekeyed_runs.emplace_back(existing->run_id, run.run_id);
                            for (auto &[_, id] : known_groups) if (id == existing->run_id) id = run.run_id;
                            for (auto &alias : result.successful_aliases)
                                if (alias.run_id == existing->run_id) alias.run_id = run.run_id;
                        }
                        if (merge == domain::ImportMerge::Added) added.insert(run.run_id);
                        if (merge == domain::ImportMerge::Enriched && !added.contains(run.run_id)) enriched.insert(run.run_id);
                        if (added.contains(run.run_id)) enriched.erase(run.run_id);
                        known_groups[key] = run.run_id;
                        for (const auto &ref : {run.sources.perf, run.sources.csv}) {
                            if (!ref) continue;
                            const auto path = candidate->sources().resolve(*ref);
                            const auto root = rootOf(*candidate, *ref);
                            if (!path || !root) continue;
                            const bool requested = (group.perf && sourcePathKey(*path) == sourcePathKey(group.perf->absolutePath())) ||
                                (group.csv && sourcePathKey(*path) == sourcePathKey(group.csv->absolutePath()));
                            if (requested) result.successful_aliases.push_back({*path, *root, ref->filename, run.run_id});
                        }
                    }
                }
                ++done;
                if (progress) progress(ProfileImportPhase::Importing, done, groups.size());
            }
            result.summary.added = added.size();
            result.summary.enriched = enriched.size();

            std::vector<std::pair<const ScannedFile *, domain::FailedSource>> failures;
            std::set<std::string> recorded;
            FailureFingerprints next;
            for (const auto &issue : result.summary.issues) {
                const auto path_key = sourcePathKey(issue.path);
                const auto file = scanned.find(path_key);
                if (file == scanned.end() || !recorded.insert(path_key).second) continue;
                const auto found = kept.find(path_key);
                auto failed = issue.previously_failed && found != kept.end() ? found->second
                    : domain::FailedSource{{}, file->second.size, file->second.modified_ms, issue.message};
                next.emplace(path_key, std::tuple{failed.size, failed.modified_ms, failed.message});
                failures.emplace_back(&file->second, std::move(failed));
            }
            // Without a baseline there is no loaded profile to update; a failures-only candidate would
            // be saved over whatever profile file has not been loaded.
            result.failures_changed = request.baseline && next != fingerprintsOf(*request.baseline);
            if (result.changed || result.failures_changed) {
                if (!result.changed) candidate = baselineCopy();
                std::vector<domain::FailedSource> failed_sources;
                failed_sources.reserve(failures.size());
                for (auto &[file, failed] : failures) {
                    failed.file = {candidate->ensureSource(file->root, file->subdir), file->filename};
                    failed_sources.push_back(std::move(failed));
                }
                candidate->setFailedSources(failed_sources);
                result.failed_sources = std::move(failed_sources);
                result.candidate = std::move(candidate);
            } else if (request.baseline) {
                result.failed_sources = request.baseline->failedSources();
            }
        } catch (const std::exception &error) {
            result.failed = true;
            result.changed = false;
            result.failures_changed = false;
            result.failed_sources.clear();
            result.candidate.reset();
            result.successful_aliases.clear();
            result.summary.issues.push_back({{}, error.what(), false});
        }
        return result;
    }
}
