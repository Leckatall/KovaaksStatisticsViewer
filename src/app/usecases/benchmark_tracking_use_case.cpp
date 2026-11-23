#include "benchmark_tracking_use_case.h"

#include <algorithm>
#include <string>
#include <utility>
#include <variant>

#include "benchmarks/benchmark_validation.h"

namespace ksv::application {
    namespace {
        // Takes the snapshot by reference: IBenchmarksService::snapshot() returns by value, so a
        // pointer into a local copy would dangle the moment that copy died.
        const data::LoadedBenchmark *findLoaded(const data::BenchmarkLibrarySnapshot &snapshot,
                                                const domain::BenchmarkId &id) {
            for (const auto &entry: snapshot.entries)
                if (entry.id == id) return std::get_if<data::LoadedBenchmark>(&entry.content);
            return nullptr;
        }

        BenchmarkChoiceClassification problemClassification(const data::BenchmarkFileProblem problem) {
            switch (problem) {
                case data::BenchmarkFileProblem::Invalid: return BenchmarkChoiceClassification::Invalid;
                case data::BenchmarkFileProblem::Unsupported: return BenchmarkChoiceClassification::Unsupported;
                case data::BenchmarkFileProblem::MigrationFailed: return BenchmarkChoiceClassification::MigrationFailed;
            }
            return BenchmarkChoiceClassification::Invalid;
        }

        BenchmarkChoice choiceFor(const data::BenchmarkFileEntry &entry) {
            BenchmarkChoice choice;
            choice.filename = entry.filename;
            choice.id = entry.id;
            choice.displayName = entry.name;
            if (const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content)) {
                choice.classification = loaded->completeness.completeness == domain::Completeness::Trackable
                                            ? BenchmarkChoiceClassification::Trackable
                                            : BenchmarkChoiceClassification::Incomplete;
                choice.selectable = true;
                return choice;
            }
            const auto &problem = std::get<data::ProblemBenchmark>(entry.content);
            choice.classification = problemClassification(problem.problem);
            choice.schemaVersion = problem.schemaVersion;
            choice.migrationFailure = problem.migrationFailure;
            choice.selectable = false;
            return choice;
        }
    }

    BenchmarkTrackingUseCase::BenchmarkTrackingUseCase(
        std::shared_ptr<data::IBenchmarksService> benchmarks,
        std::shared_ptr<IBenchmarkResolutionUseCase> resolution,
        std::shared_ptr<IProfileService> profileService,
        std::shared_ptr<IGraphConfigurationUseCase> configuration,
        std::shared_ptr<data::IDaySource> daySource,
        std::shared_ptr<IRecentBenchmarks> recent)
        : m_benchmarks(std::move(benchmarks)), m_resolution(std::move(resolution)),
          m_profileService(std::move(profileService)), m_configuration(std::move(configuration)),
          m_daySource(std::move(daySource)), m_recent(std::move(recent)) {
        const auto reevaluate = [this] {
            m_cache.clear();
            refresh();
            notifyChanged();
        };
        if (m_recent) {
            m_recent->onChanged([this] {
                if (m_markingViewed) return;
                refresh();
                notifyChanged();
            });
        }
        m_resolution->onChanged(reevaluate);
        m_configuration->onChanged([this](const GraphConfigurationChange &change) {
            const auto &rank = change.sectionsOf(GraphKey::BenchmarkRank);
            const auto &recentRank = change.sectionsOf(GraphKey::BenchmarkRecentRank);
            const auto &playtime = change.sectionsOf(GraphKey::BenchmarkPlaytime);
            if (rank.range) refreshRankOnly(GraphKey::BenchmarkRank);
            if (recentRank.range) refreshRankOnly(GraphKey::BenchmarkRecentRank);
            if (playtime.range || playtime.window) refreshPlaytimeOnly();
            // One announcement however many of the benchmark graphs restyled.
            if (rank.style || recentRank.style || playtime.style)
                for (const auto &callback: m_presentationCallbacks) callback();
        });
        m_daySource->onDayChanged([this](std::chrono::sys_days) {
            const auto committed = m_configuration->current();
            const auto follows = [&](const GraphKey graph) {
                return followsToday(effectiveRange(committed.configuration->preferences(graph)));
            };
            for (const auto graph: {GraphKey::BenchmarkRank, GraphKey::BenchmarkRecentRank})
                if (follows(graph)) refreshRankOnly(graph);
            if (follows(GraphKey::BenchmarkPlaytime)) refreshPlaytimeOnly();
        });
        refresh();
    }

    void BenchmarkTrackingUseCase::select(const domain::BenchmarkId &id) {
        if (m_selected && *m_selected == id) return;
        m_selected = id;
        m_lastKnownSelectedDisplayName.clear();
        const auto chosen = std::ranges::find(m_snapshot.choices, std::optional{id}, &BenchmarkChoice::id);
        if (m_recent && chosen != m_snapshot.choices.end()) {
            m_markingViewed = true;
            m_recent->markViewed(chosen->filename);
            m_markingViewed = false;
        }
        refresh();
        notifyChanged();
    }

    void BenchmarkTrackingUseCase::clearSelection() {
        if (!m_selected) return;
        m_selected.reset();
        m_lastKnownSelectedDisplayName.clear();
        refresh();
        notifyChanged();
    }

    void BenchmarkTrackingUseCase::selectDefault() {
        if (m_selected) return;
        const auto &choices = m_snapshot.choices;
        const auto selectable = [](const BenchmarkChoice &choice) { return choice.selectable && choice.id; };

        if (m_recent) {
            for (const auto &remembered: m_recent->filenames()) {
                const auto found = std::find_if(choices.begin(), choices.end(), [&](const BenchmarkChoice &choice) {
                    return selectable(choice) && choice.filename == remembered;
                });
                if (found == choices.end()) continue;
                // `select` rebuilds m_snapshot, which `found` points into; pass a copy.
                const domain::BenchmarkId id = *found->id;
                select(id);
                return;
            }
        }

        if (std::count_if(choices.begin(), choices.end(), selectable) != 1) return;
        const auto only = std::find_if(choices.begin(), choices.end(), selectable);
        const domain::BenchmarkId id = *only->id;
        select(id);
    }

    void BenchmarkTrackingUseCase::notifyChanged() const {
        for (const auto &callback: m_callbacks) callback();
    }

    void BenchmarkTrackingUseCase::refresh() {
        const auto revision = m_benchmarks->revision();
        if (!m_librarySnapshotValid || revision != m_librarySnapshotRevision) {
            m_librarySnapshot = m_benchmarks->snapshot();
            m_librarySnapshotRevision = revision;
            m_librarySnapshotValid = true;
        }
        const auto &library = m_librarySnapshot;

        m_snapshot = {};
        m_snapshot.libraryRevision = revision;
        m_snapshot.selectedId = m_selected;
        if (library) {
            for (const auto &entry: library->entries) m_snapshot.choices.push_back(choiceFor(entry));
        }
        if (m_recent) {
            const auto &recent = m_recent->filenames();
            for (auto &choice: m_snapshot.choices) {
                const auto found = std::ranges::find(recent, choice.filename);
                if (found != recent.end()) choice.recency = static_cast<std::size_t>(found - recent.begin());
            }
        }

        if (!m_selected) {
            m_state = BenchmarkTrackingState::NoSelection;
            m_snapshot.availability = m_state;
            return;
        }

        m_snapshot.lastKnownSelectedDisplayName = m_lastKnownSelectedDisplayName;
        for (const auto &choice: m_snapshot.choices) {
            if (choice.id == m_selected) {
                m_lastKnownSelectedDisplayName = choice.displayName;
                m_snapshot.lastKnownSelectedDisplayName = choice.displayName;
                break;
            }
        }

        const data::LoadedBenchmark *loaded = library ? findLoaded(*library, *m_selected) : nullptr;

        auto cached = m_cache.find(*m_selected);
        if (cached == m_cache.end() && loaded) {
            // Only Ready results are cached, so a benchmark that reappears is evaluated rather than
            // served a remembered absence.
            cached = m_cache.emplace(*m_selected, evaluate(*loaded)).first;
        }
        // Resolution invalidates the cache, but a recents notification can arrive before it
        // processes a library change. The cached evaluation can still be Ready in that callback.
        if (cached != m_cache.end()) {
            applyPlaytimeView(cached->second);
            applyRankRange(GraphKey::BenchmarkRank, cached->second);
            applyRankRange(GraphKey::BenchmarkRecentRank, cached->second);
            m_snapshot.benchmark.emplace(cached->second.benchmark);
            m_state = BenchmarkTrackingState::Ready;
        } else {
            m_state = BenchmarkTrackingState::Unavailable;
        }

        m_snapshot.availability = m_state;
    }

    void BenchmarkTrackingUseCase::applyPlaytimeView(Evaluation &evaluation) {
        const auto committed = m_configuration->current();
        const auto &preferences = committed.configuration->preferences(GraphKey::BenchmarkPlaytime);
        const int window = effectiveAveragingWindow(preferences);
        if (evaluation.fullPlaytimeWindowDays != window) {
            evaluation.fullPlaytime = includeInactiveDays(evaluation.benchmark.rollingPlaytime(window));
            evaluation.fullPlaytimeWindowDays = window;
        }
        const auto projection = resolveRange(effectiveRange(preferences), m_daySource->today(),
                                             evaluation.benchmark.latestRunDay());
        m_snapshot.visiblePlaytime = visiblePlaytime(evaluation.fullPlaytime, projection);
        m_snapshot.playtime = describePlaytimeView(window, projection, evaluation.fullPlaytime);
    }

    void BenchmarkTrackingUseCase::refreshPlaytimeOnly() {
        if (m_state != BenchmarkTrackingState::Ready || !m_selected) return;
        const auto cached = m_cache.find(*m_selected);
        if (cached == m_cache.end()) return;
        applyPlaytimeView(cached->second);
        notifyPlaytimeChanged();
    }

    void BenchmarkTrackingUseCase::notifyPlaytimeChanged() const {
        for (const auto &callback: m_playtimeCallbacks) callback();
    }

    void BenchmarkTrackingUseCase::applyRankRange(const GraphKey graph, const Evaluation &evaluation) {
        auto &view = graph == GraphKey::BenchmarkRecentRank ? m_snapshot.recentRank : m_snapshot.rank;
        const auto committed = m_configuration->current();
        const auto range = effectiveRange(committed.configuration->preferences(graph));
        // Both graphs share the latest member run as their anchor: the setting ends ranges at the
        // latest recorded run, and the personal-best line holds its level up to there.
        const auto &starts = evaluation.benchmark.memberRunStarts();
        const auto latestRunDay = starts.empty()
                                      ? std::nullopt
                                      : std::optional{std::chrono::floor<std::chrono::days>(starts.back())};
        const auto projection = resolveRange(range, m_daySource->today(), latestRunDay);
        view = {};
        if (range.anchorToLatestRun) view.anchoredEnd = latestRunDay;
        if (const auto *interval = std::get_if<DayInterval>(&projection)) {
            view.interval = *interval;
            // Dates running past the anchor are cut back to it; dates wholly after it stay as entered.
            const auto &end = view.anchoredEnd;
            if (end && interval->first <= *end && *end < interval->last) view.interval->last = *end;
        }
        view.noRelevantHistory = std::holds_alternative<NoRelevantHistory>(projection);
    }

    void BenchmarkTrackingUseCase::refreshRankOnly(const GraphKey graph) {
        if (m_state != BenchmarkTrackingState::Ready || !m_selected) return;
        const auto cached = m_cache.find(*m_selected);
        if (cached == m_cache.end()) return;
        applyRankRange(graph, cached->second);
        for (const auto &callback: m_rankCallbacks) callback(graph);
    }

    BenchmarkTrackingUseCase::Evaluation BenchmarkTrackingUseCase::evaluate(
        const data::LoadedBenchmark &loaded) const {
        const auto resolutionSnapshot = m_resolution->snapshot();
        const auto entry = resolutionSnapshot.resolutions.find(loaded.benchmark.id);
        const std::vector<domain::ScenarioResolution> resolutions =
            entry != resolutionSnapshot.resolutions.end() ? entry->second
                                                          : std::vector<domain::ScenarioResolution>{};
        // Unique mapped hashes only. The name is left empty deliberately: ScenarioId equality,
        // ordering and hashing all ignore it, so the profile lookup needs nothing else.
        auto hashes = loaded.benchmark.scenarioHashes();
        std::vector<domain::ScenarioId> scenarios;
        scenarios.reserve(hashes.size());
        for (auto &hash: hashes) scenarios.push_back({{}, std::move(hash)});
        const auto facts = m_profileService->getRunFacts(scenarios);
        return {domain::Benchmark{loaded.benchmark, loaded.completeness, resolutions, facts}, {}, {}};
    }
}
