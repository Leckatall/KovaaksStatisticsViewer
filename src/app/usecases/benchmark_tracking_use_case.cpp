#include "benchmark_tracking_use_case.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

#include "benchmarks/benchmark_evaluator.h"

namespace ksv::application {
    namespace {
        // Takes the snapshot by reference: IBenchmarksService::snapshot() returns by value, so a
        // pointer into a local copy would dangle the moment that copy died.
        const data::LoadedBenchmark *findLoaded(const data::BenchmarkLibrarySnapshot &snapshot,
                                                const domain::BenchmarkId &id) {
            const data::LoadedBenchmark *found = nullptr;
            for (const auto &entry: snapshot.entries) {
                const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content);
                if (!loaded || !(loaded->benchmark.id == id)) continue;
                // Two files claiming one id resolve to unavailable, never to an arbitrary pick.
                if (found != nullptr) return nullptr;
                found = loaded;
            }
            return found;
        }

        BenchmarkChoice choiceFor(const data::BenchmarkFileEntry &entry) {
            BenchmarkChoice choice;
            choice.filename = entry.filename;
            if (const auto *loaded = std::get_if<data::LoadedBenchmark>(&entry.content)) {
                choice.id = loaded->benchmark.id;
                choice.displayName = loaded->benchmark.name.empty() ? entry.filename : loaded->benchmark.name;
                choice.classification = loaded->completeness.completeness == domain::Completeness::Trackable
                                            ? BenchmarkChoiceClassification::Trackable
                                            : BenchmarkChoiceClassification::Incomplete;
                choice.selectable = true;
                return choice;
            }
            const auto &problem = std::get<data::ProblemBenchmark>(entry.content);
            choice.id = problem.id;
            choice.displayName = problem.displayName && !problem.displayName->empty()
                                     ? *problem.displayName
                                     : entry.filename;
            choice.classification = problem.problem == data::BenchmarkFileProblem::Unsupported
                                        ? BenchmarkChoiceClassification::Unsupported
                                        : BenchmarkChoiceClassification::Invalid;
            choice.schemaVersion = problem.schemaVersion;
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
        m_benchmarks->onChanged(reevaluate);
        m_resolution->onChanged(reevaluate);
        m_profileService->onProfileChanged(reevaluate);
        m_configuration->onChanged([this](const GraphConfigurationChange &change) {
            if (change.sectionsOf(GraphKey::BenchmarkRank).range) refreshRankOnly();
            const auto &playtime = change.sectionsOf(GraphKey::BenchmarkPlaytime);
            if (playtime.range || playtime.window) refreshPlaytimeOnly();
            // One announcement however many of the two graphs restyled.
            if (change.sectionsOf(GraphKey::BenchmarkRank).style || playtime.style)
                for (const auto &callback: m_presentationCallbacks) callback();
        });
        m_daySource->onDayChanged([this](std::chrono::sys_days) {
            const auto committed = m_configuration->current();
            if (followsToday(effectiveRange(committed.configuration->preferences(GraphKey::BenchmarkRank))))
                refreshRankOnly();
            if (followsToday(effectiveRange(committed.configuration->preferences(GraphKey::BenchmarkPlaytime))))
                refreshPlaytimeOnly();
        });
        refresh();
    }

    void BenchmarkTrackingUseCase::select(const domain::BenchmarkId &id) {
        if (m_selected && *m_selected == id) return;
        m_selected = id;
        m_lastKnownSelectedDisplayName.clear();
        if (m_recent) {
            m_markingViewed = true;
            m_recent->markViewed(id.value);
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
            for (const auto &remembered: m_recent->ids()) {
                const auto found = std::find_if(choices.begin(), choices.end(), [&](const BenchmarkChoice &choice) {
                    return selectable(choice) && choice.id->value == remembered;
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
            const auto &recent = m_recent->ids();
            for (auto &choice: m_snapshot.choices) {
                if (!choice.id) continue;
                const auto found = std::ranges::find(recent, choice.id->value);
                if (found != recent.end()) choice.recency = static_cast<std::size_t>(found - recent.begin());
            }
        }

        if (!m_selected) {
            m_state = BenchmarkTrackingState::NoSelection;
            m_projection = {};
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
        // Any library or profile change drops the whole cache, so a surviving entry is always
        // current for the definition it was built from -- presence is the only check a hit needs.
        if (cached != m_cache.end()) {
            m_projection = cached->second.projection;
            applyPlaytimeView(cached->second);
            applyRankRange(cached->second);
            m_state = BenchmarkTrackingState::Ready;
        } else {
            m_state = BenchmarkTrackingState::Unavailable;
            m_projection = {};
        }

        m_snapshot.availability = m_state;
        if (m_state == BenchmarkTrackingState::Ready) {
            if (loaded) m_snapshot.selectedLoaded = *loaded;
            m_snapshot.projection = m_projection;
        }
    }

    void BenchmarkTrackingUseCase::applyPlaytimeView(Evaluation &evaluation) {
        const auto committed = m_configuration->current();
        const auto &preferences = committed.configuration->preferences(GraphKey::BenchmarkPlaytime);
        const int window = effectiveAveragingWindow(preferences);
        if (evaluation.fullPlaytimeWindowDays != window) {
            evaluation.fullPlaytime = includeInactiveDays(
                m_profileService->getRollingTimeAverage(evaluation.scenarios, window));
            evaluation.fullPlaytimeWindowDays = window;
        }
        const auto projection = resolveRange(effectiveRange(preferences), m_daySource->today(),
                                             evaluation.latestRunDay);
        m_projection.rollingPlaytime = visiblePlaytime(evaluation.fullPlaytime, projection);
        m_snapshot.playtime = describePlaytimeView(window, projection, evaluation.fullPlaytime);
    }

    void BenchmarkTrackingUseCase::refreshPlaytimeOnly() {
        if (m_state != BenchmarkTrackingState::Ready || !m_selected) return;
        const auto cached = m_cache.find(*m_selected);
        if (cached == m_cache.end()) return;
        applyPlaytimeView(cached->second);
        m_snapshot.projection->rollingPlaytime = m_projection.rollingPlaytime;
        notifyPlaytimeChanged();
    }

    void BenchmarkTrackingUseCase::notifyPlaytimeChanged() const {
        for (const auto &callback: m_playtimeCallbacks) callback();
    }

    void BenchmarkTrackingUseCase::applyRankRange(const Evaluation &evaluation) {
        const auto committed = m_configuration->current();
        const auto range = effectiveRange(committed.configuration->preferences(GraphKey::BenchmarkRank));
        // Anchored at the last improvement, not the newest run: the rank series has no point after it.
        const auto &history = evaluation.projection.averageRankHistory;
        const auto latestImprovementDay = history.empty() ? std::nullopt : std::optional{history.back().day};
        const auto projection = resolveRange(range, m_daySource->today(), latestImprovementDay);
        m_snapshot.rank = {};
        if (range.anchorToLatestRun) m_snapshot.rank.anchoredEnd = latestImprovementDay;
        if (const auto *interval = std::get_if<DayInterval>(&projection)) {
            m_snapshot.rank.interval = *interval;
            // Dates running past the last point are cut back to it; dates wholly after it stay as entered.
            const auto &end = m_snapshot.rank.anchoredEnd;
            if (end && interval->first <= *end && *end < interval->last) m_snapshot.rank.interval->last = *end;
        }
        m_snapshot.rank.noRelevantHistory = std::holds_alternative<NoRelevantHistory>(projection);
    }

    void BenchmarkTrackingUseCase::refreshRankOnly() {
        if (m_state != BenchmarkTrackingState::Ready || !m_selected) return;
        const auto cached = m_cache.find(*m_selected);
        if (cached == m_cache.end()) return;
        applyRankRange(cached->second);
        for (const auto &callback: m_rankCallbacks) callback();
    }

    BenchmarkTrackingUseCase::Evaluation BenchmarkTrackingUseCase::evaluate(
        const data::LoadedBenchmark &loaded) const {
        const auto resolutionSnapshot = m_resolution->snapshot();
        const auto entry = resolutionSnapshot.resolutions.find(loaded.benchmark.id);
        const std::vector<domain::ScenarioResolution> resolutions =
            entry != resolutionSnapshot.resolutions.end() ? entry->second
                                                          : std::vector<domain::ScenarioResolution>{};
        // Unique resolved hashes only. The name is left empty deliberately: ScenarioId equality,
        // ordering and hashing all ignore it, so the profile lookup needs nothing else.
        std::vector<domain::ScenarioId> scenarios;
        std::unordered_set<std::string> hashes;
        for (const auto &resolution: resolutions) {
            for (const auto &hash: domain::mappedScenarioHashes(resolution.hash, resolution.additionalHashes))
                if (hashes.insert(hash).second) scenarios.push_back({{}, hash});
        }
        const auto facts = m_profileService->getRunFacts(scenarios);
        Evaluation evaluation;
        for (const auto &fact: facts) {
            // The playtime accounting skips runs stamped at or before the epoch, so the anchor does too.
            if (fact.run_id.start_time <= 0) continue;
            const auto day = fact.run_id.startDay();
            if (!evaluation.latestRunDay || day > *evaluation.latestRunDay) evaluation.latestRunDay = day;
        }
        evaluation.projection = domain::evaluateBenchmark(loaded.benchmark, loaded.completeness, resolutions,
                                                          facts, {});
        evaluation.scenarios = std::move(scenarios);
        return evaluation;
    }
}
