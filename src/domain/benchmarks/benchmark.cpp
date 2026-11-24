#include "benchmarks/benchmark.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

#include "rolling_playtime.h"

namespace ksv::domain {
    namespace {
        double normalizedFromLadder(const std::optional<std::vector<double> > &ladder,
                                    const std::optional<double> score) {
            if (!ladder || !score || !std::isfinite(*score)) return 0.0;
            const double value = *score;
            if (value < ladder->front())
                return ladder->front() > 0.0 ? std::max(0.0, value / ladder->front()) : 0.0;
            for (std::size_t index = 1; index < ladder->size(); ++index)
                if (value < (*ladder)[index])
                    return static_cast<double>(index) +
                           (value - (*ladder)[index - 1]) / ((*ladder)[index] - (*ladder)[index - 1]);
            return static_cast<double>(ladder->size());
        }
    }

    const BenchmarkScenarioId &ScenarioBenchmarkEvaluation::entryId() const { return m_entry.id; }
    const std::string &ScenarioBenchmarkEvaluation::name() const { return m_entry.name; }
    ScenarioMatchState ScenarioBenchmarkEvaluation::matchState() const { return m_matchState; }
    const std::optional<ScenarioEvaluation> &ScenarioBenchmarkEvaluation::stats() const { return m_stats; }
    std::optional<TierId> ScenarioBenchmarkEvaluation::attainedTier() const { return m_attainedTier; }
    std::optional<Threshold> ScenarioBenchmarkEvaluation::nextThreshold() const { return m_nextThreshold; }
    double ScenarioBenchmarkEvaluation::normalizedRank() const { return m_normalizedRank; }
    double ScenarioBenchmarkEvaluation::normalizedRecentRank() const { return m_normalizedRecentRank; }

    std::vector<NormalizedPersonalBest> ScenarioBenchmarkEvaluation::normalizedPersonalBestHistory() const {
        std::vector<NormalizedPersonalBest> result;
        if (!m_stats) return result;
        result.reserve(m_stats->personalBestHistory().size());
        for (const auto &[scenarioRun, score]: m_stats->personalBestHistory())
            result.push_back({scenarioRun, normalized(score)});
        return result;
    }

    std::vector<double> ScenarioBenchmarkEvaluation::normalizedRecentAverages() const {
        std::vector<double> result;
        if (!m_stats) return result;
        result.reserve(m_stats->recentAverages().size());
        for (const auto average: m_stats->recentAverages()) result.push_back(normalized(average));
        return result;
    }

    std::optional<double> ScenarioBenchmarkEvaluation::thresholdFor(const TierId &tier) const {
        return m_entry.thresholdFor(tier);
    }

    const std::optional<std::vector<double>> &ScenarioBenchmarkEvaluation::ladder() const { return m_ladder; }

    bool ScenarioBenchmarkEvaluation::attains(const std::size_t tierIndex) const {
        // Keep an unplayed entry distinct from a recorded zero when the first threshold is zero.
        return m_stats && m_ladder && tierIndex < m_ladder->size() && m_stats->personalBest() >= (*m_ladder)[tierIndex];
    }

    double ScenarioBenchmarkEvaluation::normalized(const std::optional<double> score) const {
        return normalizedFromLadder(m_ladder, score);
    }

    namespace {
        template<typename T>
        void appendPointers(const std::vector<T> &values, std::vector<const T *> &pointers) {
            for (const auto &value: values) pointers.push_back(&value);
        }

        template<typename T>
        std::vector<const T *> pointersTo(const std::vector<T> &values) {
            std::vector<const T *> pointers;
            pointers.reserve(values.size());
            appendPointers(values, pointers);
            return pointers;
        }

        const ScenarioBenchmarkEvaluation *findScenario(const std::vector<ScenarioBenchmarkEvaluation> &scenarios,
                                                        const BenchmarkScenarioId &id) {
            const auto found = std::ranges::find(scenarios, id, &ScenarioBenchmarkEvaluation::entryId);
            return found == scenarios.end() ? nullptr : &*found;
        }
    }

    const GroupId &GroupEvaluation::id() const { return m_id; }
    const std::string &GroupEvaluation::name() const { return m_name; }
    BenchmarkColor GroupEvaluation::color() const { return m_color; }
    std::optional<TierId> GroupEvaluation::satisfiedTier() const { return m_satisfiedTier; }
    std::vector<const ScenarioBenchmarkEvaluation *> GroupEvaluation::scenarios() const {
        return pointersTo(m_scenarios);
    }
    std::vector<const GroupEvaluation *> GroupEvaluation::subgroups() const { return pointersTo(m_subgroups); }

    namespace {
        // ---- Run indexing ----------------------------------------------------------------------

        using EntriesByHash = std::unordered_map<std::string, std::vector<std::size_t>>;
        using MatchByEntry = std::unordered_map<std::string, ScenarioMatchState>;

        struct RunIndex {
            EntriesByHash entriesByHash;
            std::vector<std::vector<TimedScore>> histories;
        };

        RunIndex indexRuns(const std::vector<const BenchmarkScenario *> &entries, const MatchByEntry &matches,
                           std::span<const RunFact> runFacts, std::vector<TimedScore> &memberScores) {
            RunIndex index{{}, std::vector<std::vector<TimedScore>>(entries.size())};
            for (std::size_t entry = 0; entry < entries.size(); ++entry) {
                const auto match = matches.find(entries[entry]->id.value);
                if (match == matches.end() || match->second != ScenarioMatchState::Resolved) continue;
                for (const auto &hash: entries[entry]->hashes) {
                    auto &bucket = index.entriesByHash[hash];
                    if (bucket.empty() || bucket.back() != entry) bucket.push_back(entry);
                }
            }

            // Keep both sequences chronological and retain input order when start times tie.
            for (const auto &fact: runFacts) {
                const auto bucket = index.entriesByHash.find(fact.run_id.scenario_id.hash);
                if (bucket == index.entriesByHash.end()) continue;
                for (const auto entry: bucket->second) {
                    const auto timed = fact.timed();
                    index.histories[entry].push_back(timed);
                    memberScores.push_back(timed);
                }
            }
            return index;
        }

        struct StateRange {
            std::size_t begin;
            std::size_t count;
        };

        // Met when any one of its scenarios attains the tier.
        struct Requirement {
            GroupId group;
            StateRange scenarios;
        };

        // Subcategories stand in for a category's direct scenarios when it has both.
        struct CategoryLayout {
            std::vector<Requirement> requirements;
        };

        struct ScenarioTree {
            std::vector<ScenarioBenchmarkEvaluation> states;
            StateRange uncategorized;
            std::vector<CategoryLayout> categories;
        };

        ScenarioTree layoutScenarios(const BenchmarkDefinition &definition,
                                     std::vector<ScenarioBenchmarkEvaluation> states) {
            std::size_t offset = 0;
            const auto rangeFor = [&](const std::size_t count) {
                const StateRange range{offset, count};
                offset += count;
                return range;
            };

            ScenarioTree tree{std::move(states), rangeFor(definition.uncategorized.size()), {}};
            for (const auto &category: definition.categories) {
                CategoryLayout layout;
                const auto direct = rangeFor(category.scenarios.size());
                for (const auto &subcategory: category.subcategories)
                    layout.requirements.push_back({subcategory.id, rangeFor(subcategory.scenarios.size())});
                if (category.subcategories.empty()) layout.requirements.push_back({category.id, direct});
                tree.categories.push_back(std::move(layout));
            }
            return tree;
        }

        // ---- Tier rules ------------------------------------------------------------------------

        struct TierRules {
            const std::vector<Tier> &tiers;
            const ScenarioTree &tree;

            [[nodiscard]] std::span<const ScenarioBenchmarkEvaluation> slice(const StateRange range) const {
                return std::span(tree.states).subspan(range.begin, range.count);
            }

            [[nodiscard]] bool met(const Requirement &requirement, const std::size_t tier) const {
                return std::ranges::any_of(slice(requirement.scenarios),
                                           [&](const auto &state) { return state.attains(tier); });
            }

            [[nodiscard]] bool satisfied(const CategoryLayout &category, const std::size_t tier) const {
                return std::ranges::all_of(category.requirements,
                                           [&](const Requirement &requirement) { return met(requirement, tier); });
            }

            [[nodiscard]] bool allAttain(const std::span<const ScenarioBenchmarkEvaluation> states, const std::size_t tier) const {
                return std::ranges::all_of(states, [&](const ScenarioBenchmarkEvaluation &state) { return state.attains(tier); });
            }

            [[nodiscard]] bool rankAttained(const std::size_t tier) const {
                return allAttain(slice(tree.uncategorized), tier) &&
                       std::ranges::all_of(tree.categories,
                                           [&](const CategoryLayout &category) { return satisfied(category, tier); });
            }

            [[nodiscard]] bool completed(const std::size_t tier) const { return allAttain(tree.states, tier); }

            template<typename Predicate>
            [[nodiscard]] std::optional<std::size_t> highestIndex(const Predicate &predicate) const {
                for (std::size_t index = tiers.size(); index-- > 0;)
                    if (predicate(index)) return index;
                return std::nullopt;
            }

            template<typename Predicate>
            [[nodiscard]] std::optional<TierId> highestTier(const Predicate &predicate) const {
                const auto index = highestIndex(predicate);
                return index ? std::optional(tiers[*index].id) : std::nullopt;
            }
        };

        RankBlockerScenario blockerScenario(const ScenarioBenchmarkEvaluation &state, const std::size_t tier) {
            const auto &ladder = state.ladder();
            const auto &stats = state.stats();
            return {state.entryId(), ladder ? (*ladder)[tier] : 0.0,
                    stats ? std::optional(stats->personalBest()) : std::nullopt};
        }

        // ---- Rank history ----------------------------------------------------------------------

        struct BenchmarkScenarioCursor {
            std::vector<NormalizedPersonalBest> bests;
            std::vector<double> recents;
            std::size_t nextBest = 0;
            int runs = 0;
            double best = 0.0;
            double recent = 0.0;
        };

        // runFacts is chronological by contract (UserProfile::getRunFacts), and indexRuns walked it
        // through the same entriesByHash, so a BenchmarkScenario's k-th member run here is run k of
        // the sequence its stats were built from. Each point re-averages every BenchmarkScenario in
        // tree order from 0.0, exactly as averageRank() does, so the histories end bit for bit on the
        // current means instead of drifting the way running sums would.
        void sweepRankHistory(std::span<const RunFact> runFacts, const EntriesByHash &entriesByHash,
                              const std::vector<ScenarioBenchmarkEvaluation> &states,
                              std::vector<AverageRankAfterRun> &bestHistory,
                              std::vector<AverageRankAfterRun> &recentHistory,
                              std::vector<std::chrono::sys_seconds> &memberRunStarts) {
            std::vector<BenchmarkScenarioCursor> cursors;
            cursors.reserve(states.size());
            for (const auto &state: states)
                cursors.push_back({state.normalizedPersonalBestHistory(), state.normalizedRecentAverages()});
            const auto meanOf = [&](double BenchmarkScenarioCursor::*value) {
                double total = 0.0;
                for (const auto &cursor: cursors) total += cursor.*value;
                return total / static_cast<double>(cursors.size());
            };

            for (const auto &fact: runFacts) {
                const auto bucket = entriesByHash.find(fact.run_id.scenario_id.hash);
                if (bucket == entriesByHash.end()) continue;
                const auto start = fact.run_id.startSecond();
                memberRunStarts.push_back(start);
                const auto benchmarkRun = static_cast<int>(memberRunStarts.size());
                bool improved = false;
                for (const auto entry: bucket->second) {
                    auto &cursor = cursors[entry];
                    const int run = ++cursor.runs;
                    cursor.recent = cursor.recents[static_cast<std::size_t>(run - 1)];
                    if (cursor.nextBest < cursor.bests.size() && cursor.bests[cursor.nextBest].scenarioRun == run) {
                        cursor.best = cursor.bests[cursor.nextBest++].normalizedRank;
                        improved = true;
                    }
                }
                if (improved) bestHistory.push_back({start, benchmarkRun, meanOf(&BenchmarkScenarioCursor::best)});
                recentHistory.push_back({start, benchmarkRun, meanOf(&BenchmarkScenarioCursor::recent)});
            }
        }
    }

    struct Benchmark::Builder {
        Benchmark &result;

        void evaluate(const std::vector<ScenarioResolution> &resolutions, std::span<const RunFact> runFacts);
        ScenarioBenchmarkEvaluation evaluateEntry(const BenchmarkScenario &entry, ScenarioMatchState match,
                                                 std::span<const TimedScore> history) const;
        void assignNextTier(const TierRules &rules, std::size_t tier) const;

        struct GroupTiers {
            std::optional<TierId> category;
            std::vector<std::optional<TierId>> subgroups;
        };

        void assembleTree(std::vector<ScenarioBenchmarkEvaluation> states, const std::vector<GroupTiers> &groupTiers);
    };

    void Benchmark::Builder::evaluate(const std::vector<ScenarioResolution> &resolutions,
                                      std::span<const RunFact> runFacts) {
        const auto &definition = result.m_definition;
        MatchByEntry matches;
        for (const auto &resolution: resolutions) matches.emplace(resolution.entryId.value, resolution.state);

        const auto entries = definition.entries();
        const auto runs = indexRuns(entries, matches, runFacts, result.m_memberScores);
        std::vector<ScenarioBenchmarkEvaluation> states;
        states.reserve(entries.size());
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const auto &entry = *entries[index];
            // A missing resolution is legal even when the caller supplies run facts.
            const auto match = matches.find(entry.id.value);
            states.push_back(evaluateEntry(entry, match == matches.end() ? ScenarioMatchState::Unresolved : match->second,
                                           runs.histories[index]));
            if (const auto &stats = states.back().stats()) result.m_totalPlaytimeSeconds += stats->playtimeSeconds();
        }
        for (const auto &run: result.m_memberScores) {
            if (run.start_time <= 0) continue;
            const auto day = run.startDay();
            if (!result.m_latestRunDay || day > *result.m_latestRunDay) result.m_latestRunDay = day;
        }

        auto tree = layoutScenarios(definition, std::move(states));
        std::vector<GroupTiers> groupTiers;
        // Partial thresholds could produce a plausible-looking rank. An empty tree would make the average undefined.
        if (result.m_completeness.completeness == Completeness::Trackable && !tree.states.empty()) {
            const TierRules rules{definition.tiers, tree};
            const auto attained = rules.highestIndex([&](std::size_t tier) { return rules.rankAttained(tier); });
            if (attained) result.m_attainedRank = definition.tiers[*attained].id;
            result.m_completedRank = rules.highestTier([&](std::size_t tier) { return rules.completed(tier); });
            double totalRank = 0.0;
            for (const auto &state: tree.states) totalRank += state.normalizedRank();
            result.m_averageRank = totalRank / static_cast<double>(tree.states.size());

            if (const std::size_t next = attained ? *attained + 1 : 0; next < definition.tiers.size())
                assignNextTier(rules, next);

            for (std::size_t index = 0; index < tree.categories.size(); ++index) {
                const auto &layout = tree.categories[index];
                GroupTiers tiers{rules.highestTier([&](std::size_t tier) { return rules.satisfied(layout, tier); }), {}};
                for (std::size_t subgroup = 0; subgroup < definition.categories[index].subcategories.size(); ++subgroup)
                    tiers.subgroups.push_back(rules.highestTier([&](std::size_t tier) {
                        return rules.met(layout.requirements[subgroup], tier);
                    }));
                groupTiers.push_back(std::move(tiers));
            }
            sweepRankHistory(runFacts, runs.entriesByHash, tree.states, result.m_averageRankHistory,
                             result.m_recentAverageRankHistory, result.m_memberRunStarts);
        }
        assembleTree(std::move(tree.states), groupTiers);
    }

    ScenarioBenchmarkEvaluation Benchmark::Builder::evaluateEntry(const BenchmarkScenario &entry,
                                                                  const ScenarioMatchState match,
                                                                  std::span<const TimedScore> history) const {
        ScenarioBenchmarkEvaluation scenario;
        scenario.m_entry = entry;
        scenario.m_matchState = match;
        scenario.m_stats = ScenarioEvaluation::fromRuns(history);
        const auto &definition = result.m_definition;
        scenario.m_ladder = definition.ladderFor(entry);
        if (scenario.m_stats) {
            scenario.m_normalizedRank = scenario.normalized(scenario.m_stats->personalBest());
            scenario.m_normalizedRecentRank = scenario.normalized(scenario.m_stats->recentAverage());
        }
        if (!scenario.m_ladder) return scenario;

        for (std::size_t tier = 0; tier < definition.tiers.size(); ++tier) {
            if (scenario.attains(tier)) scenario.m_attainedTier = definition.tiers[tier].id;
            else {
                scenario.m_nextThreshold = Threshold{definition.tiers[tier].id, (*scenario.m_ladder)[tier]};
                break;
            }
        }
        return scenario;
    }

    void Benchmark::Builder::assignNextTier(const TierRules &rules, const std::size_t tier) const {
        result.m_nextTier = rules.tiers[tier].id;
        result.m_scenariosAtNextTier = static_cast<int>(std::ranges::count_if(
            rules.tree.states, [&](const auto &state) { return state.attains(tier); }));
        auto &blockers = result.m_nextTierBlockers;
        for (const auto &state: rules.slice(rules.tree.uncategorized))
            if (!state.attains(tier)) blockers.push_back({std::nullopt, {blockerScenario(state, tier)}});
        for (const auto &category: rules.tree.categories)
            for (const auto &requirement: category.requirements) {
                if (rules.met(requirement, tier)) continue;
                RankBlocker blocker{requirement.group, {}};
                for (const auto &state: rules.slice(requirement.scenarios))
                    blocker.scenarios.push_back(blockerScenario(state, tier));
                blockers.push_back(std::move(blocker));
            }
    }

    void Benchmark::Builder::assembleTree(std::vector<ScenarioBenchmarkEvaluation> states,
                                          const std::vector<GroupTiers> &groupTiers) {
        auto first = std::make_move_iterator(states.begin());
        const auto takeScenarios = [&](const std::size_t count) {
            const auto last = first + static_cast<std::ptrdiff_t>(count);
            std::vector<ScenarioBenchmarkEvaluation> scenarios(first, last);
            first = last;
            return scenarios;
        };
        const auto buildGroup = [&](const BenchmarkGroup &definition) {
            GroupEvaluation group;
            group.m_id = definition.id;
            group.m_name = definition.name;
            group.m_color = definition.color;
            group.m_scenarios = takeScenarios(definition.scenarios.size());
            return group;
        };

        const auto &definition = result.m_definition;
        result.m_uncategorized = takeScenarios(definition.uncategorized.size());
        for (std::size_t index = 0; index < definition.categories.size(); ++index) {
            const auto &category = definition.categories[index];
            auto group = buildGroup(category);
            if (!groupTiers.empty()) group.m_satisfiedTier = groupTiers[index].category;
            for (std::size_t child = 0; child < category.subcategories.size(); ++child) {
                auto subgroup = buildGroup(category.subcategories[child]);
                if (!groupTiers.empty()) subgroup.m_satisfiedTier = groupTiers[index].subgroups[child];
                group.m_subgroups.push_back(std::move(subgroup));
            }
            result.m_categories.push_back(std::move(group));
        }
    }

    Benchmark::Benchmark(BenchmarkDefinition definition, CompletenessResult completeness,
                         const std::vector<ScenarioResolution> &resolutions, std::span<const RunFact> runFacts)
        : m_definition(std::move(definition)), m_completeness(std::move(completeness)) {
        Builder{*this}.evaluate(resolutions, runFacts);
    }

    const BenchmarkDefinition &Benchmark::definition() const { return m_definition; }
    const BenchmarkId &Benchmark::id() const { return m_definition.id; }
    const CompletenessResult &Benchmark::completeness() const { return m_completeness; }
    const std::vector<Tier> &Benchmark::tiers() const { return m_definition.tiers; }

    std::vector<const ScenarioBenchmarkEvaluation *> Benchmark::uncategorized() const {
        return pointersTo(m_uncategorized);
    }
    std::vector<const GroupEvaluation *> Benchmark::categories() const { return pointersTo(m_categories); }

    std::vector<const ScenarioBenchmarkEvaluation *> Benchmark::scenarios() const {
        auto result = uncategorized();
        for (const auto &category: m_categories) {
            appendPointers(category.m_scenarios, result);
            for (const auto &subgroup: category.m_subgroups) appendPointers(subgroup.m_scenarios, result);
        }
        return result;
    }

    const ScenarioBenchmarkEvaluation *Benchmark::scenario(const BenchmarkScenarioId &id) const {
        if (const auto *found = findScenario(m_uncategorized, id)) return found;
        for (const auto &category: m_categories) {
            if (const auto *found = findScenario(category.m_scenarios, id)) return found;
            for (const auto &subgroup: category.m_subgroups)
                if (const auto *found = findScenario(subgroup.m_scenarios, id)) return found;
        }
        return nullptr;
    }

    std::optional<TierId> Benchmark::attainedRank() const { return m_attainedRank; }
    std::optional<TierId> Benchmark::completedRank() const { return m_completedRank; }
    std::optional<TierId> Benchmark::nextTier() const { return m_nextTier; }
    const std::vector<RankBlocker> &Benchmark::nextTierBlockers() const { return m_nextTierBlockers; }
    int Benchmark::scenariosAtNextTier() const { return m_scenariosAtNextTier; }
    std::optional<double> Benchmark::averageRank() const { return m_averageRank; }
    const std::vector<AverageRankAfterRun> &Benchmark::averageRankHistory() const { return m_averageRankHistory; }
    const std::vector<AverageRankAfterRun> &Benchmark::recentAverageRankHistory() const {
        return m_recentAverageRankHistory;
    }
    const std::vector<std::chrono::sys_seconds> &Benchmark::memberRunStarts() const { return m_memberRunStarts; }

    double Benchmark::totalPlaytimeSeconds() const { return m_totalPlaytimeSeconds; }
    std::optional<std::chrono::sys_days> Benchmark::latestRunDay() const { return m_latestRunDay; }
    std::vector<std::pair<std::chrono::sys_days, double>> Benchmark::rollingPlaytime(const int windowDays) const {
        return domain::rollingPlaytime(m_memberScores, windowDays);
    }
}
