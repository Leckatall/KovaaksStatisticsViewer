#include "benchmark_manager_use_case.h"

#include <algorithm>
#include <utility>
#include <variant>

#include <QString>

#include "benchmarks/benchmark_definition.h"

namespace ksv::application {
    namespace {
        // Collapses duplicate publications: the accepted service and the resolution use case both
        // notify after one reconciliation batch with nothing a manager consumer observes changing
        // between them. libraryRevision carries accepted-snapshot identity, so the snapshot itself
        // is never deep-compared here; every other field is compared by value so a new field is
        // picked up without editing this.
        bool sameState(const BenchmarkManagerState &a, const BenchmarkManagerState &b) {
            return a.libraryRevision == b.libraryRevision
                   && a.library.has_value() == b.library.has_value()
                   && a.refreshFailed == b.refreshFailed
                   && a.managedDirectoryPath == b.managedDirectoryPath
                   && a.scenarioCatalogue == b.scenarioCatalogue
                   && a.resolutions == b.resolutions
                   && a.automaticWriteFailures == b.automaticWriteFailures
                   && a.resolutionWriteFailed == b.resolutionWriteFailed;
        }
    }

    BenchmarkManagerUseCase::BenchmarkManagerUseCase(
        std::shared_ptr<data::IBenchmarksService> benchmarks,
        std::shared_ptr<IBenchmarkResolutionUseCase> resolution,
        std::shared_ptr<IPlaylistReader> playlist, std::function<std::string()> idFactory,
        std::shared_ptr<IRecentBenchmarks> recent)
        : m_benchmarks(std::move(benchmarks)), m_resolution(std::move(resolution)),
          m_playlist(std::move(playlist)), m_idFactory(std::move(idFactory)),
          m_recent(std::move(recent)) {
        m_benchmarks->onChanged([this] { rebuildAndNotify(); });
        m_resolution->onChanged([this] { rebuildAndNotify(); });
        m_state = build();
    }

    BenchmarkManagerState BenchmarkManagerUseCase::build() const {
        BenchmarkManagerState state;
        state.library = m_benchmarks->snapshot();
        state.libraryRevision = m_benchmarks->revision();
        state.refreshFailed = m_benchmarks->lastRefreshFailed();
        state.managedDirectoryPath = m_benchmarks->managedDirectoryPath();
        const auto resolution = m_resolution->snapshot();
        state.scenarioCatalogue = resolution.scenarioCatalogue;
        state.resolutions = resolution.resolutions;
        state.automaticWriteFailures = resolution.automaticWriteFailures;
        state.resolutionWriteFailed = resolution.anyAutomaticWriteFailed();
        return state;
    }

    void BenchmarkManagerUseCase::rebuildAndNotify() {
        auto next = build();
        if (sameState(next, m_state)) return;
        m_state = std::move(next);
        for (const auto &callback: m_callbacks) callback();
    }

    BenchmarkEditorSeed BenchmarkManagerUseCase::beginNewBenchmark() {
        domain::BenchmarkDefinition fresh;
        fresh.id = domain::BenchmarkId{newId()};
        m_resolution->setEditLease(fresh.id);
        return {std::move(fresh), std::nullopt};
    }

    std::optional<BenchmarkEditorSeed> BenchmarkManagerUseCase::openBenchmark(const domain::BenchmarkId &id) {
        const auto snapshot = m_benchmarks->snapshot();
        if (!snapshot) return std::nullopt;
        for (const auto &entry: snapshot->entries) {
            const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content);
            if (!loaded || !(loaded->benchmark.id == id)) continue;
            m_resolution->setEditLease(id);
            return BenchmarkEditorSeed{loaded->benchmark,
                                       data::BenchmarkEditToken{id, entry.filename, entry.digest}};
        }
        return std::nullopt;
    }

    void BenchmarkManagerUseCase::closeEditor() { m_resolution->setEditLease(std::nullopt); }

    PlaylistSeedImport BenchmarkManagerUseCase::importPlaylistSeed(const std::string &path) {
        const auto read = m_playlist->read(path);
        if (!read.seed) {
            // The port contract is seed-xor-failure; a port returning neither is treated as "no
            // usable scenario list" rather than as a phantom success.
            return {read.failure ? read.failure
                                 : std::optional<PlaylistImportFailure>{
                                       PlaylistImportFailure::NoUsableScenarioList},
                    std::nullopt, {}};
        }
        domain::BenchmarkDefinition imported;
        imported.id = domain::BenchmarkId{newId()};
        if (read.seed->name) imported.name = *read.seed->name;
        for (const auto &scenarioName: read.seed->scenarioNames)
            imported.uncategorized.push_back(domain::BenchmarkScenario{
                domain::BenchmarkScenarioId{newId()}, scenarioName, {}, {}});
        m_resolution->setEditLease(imported.id);
        return {std::nullopt, BenchmarkEditorSeed{std::move(imported), std::nullopt},
                read.seed->skippedDuplicateIndices};
    }

    std::optional<BenchmarkNameProblem> BenchmarkManagerUseCase::checkName(
        const std::string &name, const std::optional<data::BenchmarkEditToken> &token) const {
        if (const auto problem = data::files::checkName(name, data::kBenchmarkFileExtension)) {
            switch (*problem) {
                case data::files::FileNameProblem::Blank: return BenchmarkNameProblem::Blank;
                case data::files::FileNameProblem::InvalidCharacter: return BenchmarkNameProblem::InvalidCharacter;
                case data::files::FileNameProblem::TooLong: return BenchmarkNameProblem::TooLong;
                case data::files::FileNameProblem::TrailingDotOrSpace: return BenchmarkNameProblem::TrailingDotOrSpace;
                case data::files::FileNameProblem::Reserved: return BenchmarkNameProblem::Reserved;
            }
        }
        if (!m_state.library) return std::nullopt;
        const auto candidate = QString::fromStdString(name);
        const bool taken = std::ranges::any_of(m_state.library->entries, [&](const data::BenchmarkFileEntry &entry) {
            return !(token && entry.filename == token->filename) && QString::fromStdString(entry.name).compare(candidate, Qt::CaseInsensitive) == 0;
        });
        return taken ? std::optional{BenchmarkNameProblem::Taken} : std::nullopt;
    }

    data::BenchmarkSaveOutcome BenchmarkManagerUseCase::save(
        const domain::BenchmarkDefinition &benchmark, const std::optional<data::BenchmarkEditToken> &token) {
        auto outcome = m_benchmarks->save({benchmark, token});
        if (!m_recent || !outcome.ok()) return outcome;
        if (!token) m_recent->markCreated(outcome.token->filename);
        else if (outcome.token->filename != token->filename) m_recent->rename(token->filename, outcome.token->filename);
        return outcome;
    }

    std::vector<domain::ScenarioResolution> BenchmarkManagerUseCase::resolve(
        const domain::BenchmarkDefinition &benchmark) const {
        return m_resolution->resolve(benchmark);
    }

    domain::BenchmarkDefinition BenchmarkManagerUseCase::withAutomaticMappings(const domain::BenchmarkDefinition &benchmark) const {
        return m_resolution->withAutomaticMappings(benchmark);
    }

    data::BenchmarkRemoveOutcome BenchmarkManagerUseCase::deleteBenchmark(
        const data::BenchmarkEditToken &token) {
        return m_benchmarks->remove(token);
    }
}
