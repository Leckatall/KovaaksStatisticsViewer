#include "benchmarks/benchmark_evaluator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <numeric>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

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

    std::optional<std::vector<double> > thresholdLadder(const std::vector<Tier> &tiers,
                                                        const std::vector<Threshold> &thresholds) {
        if (tiers.empty()) return std::nullopt;
        std::vector<double> ordered;
        ordered.reserve(tiers.size());
        for (const auto &tier: tiers) {
            const auto matches = std::ranges::count_if(thresholds, [&](const Threshold &threshold) {
                return threshold.tierId == tier.id;
            });
            if (matches != 1) return std::nullopt;
            const auto threshold = std::ranges::find_if(thresholds, [&](const Threshold &candidate) {
                return candidate.tierId == tier.id;
            });
            if (!std::isfinite(threshold->score) || threshold->score < 0.0) return std::nullopt;
            ordered.push_back(threshold->score);
        }
        for (std::size_t index = 1; index < ordered.size(); ++index)
            if (ordered[index] <= ordered[index - 1]) return std::nullopt;
        return ordered;
    }

    double normalizedTierValue(const std::vector<Tier> &tiers, const std::vector<Threshold> &thresholds,
                               const std::optional<double> score) {
        if (!score || !std::isfinite(*score)) return 0.0;
        return normalizedFromLadder(thresholdLadder(tiers, thresholds), score);
    }

    namespace {
        using ResolutionByEntry = std::unordered_map<std::string, const ScenarioResolution *>;
        using EntriesByHash = std::unordered_map<std::string, std::vector<std::size_t> >;
        using History = std::vector<const RunFact *>;
        constexpr std::size_t kRecentRuns = 5;

        // A missing resolution is legal: callers may evaluate with no resolutions at all.
        const ScenarioResolution *findResolution(const ResolutionByEntry &resolutions, const ScenarioEntryId &id) {
            const auto found = resolutions.find(id.value);
            return found != resolutions.end() ? found->second : nullptr;
        }

        // Traversal order shared by projection and every index-parallel structure built from it.
        std::vector<const ScenarioEntry *> collectEntries(const BenchmarkDefinition &definition) {
            std::vector<const ScenarioEntry *> ordered;
            const auto append = [&](const std::vector<ScenarioEntry> &entries) {
                for (const auto &entry: entries) ordered.push_back(&entry);
            };
            append(definition.uncategorized);
            for (const auto &category: definition.categories) {
                append(category.scenarios);
                for (const auto &subcategory: category.subcategories) append(subcategory.scenarios);
            }
            return ordered;
        }

        ScenarioProjection projectScenario(const ScenarioEntry &entry, const std::vector<Tier> &tiers,
                                           const ScenarioResolution *resolution, const History &history,
                                           const std::optional<std::vector<double> > &ladder) {
            ScenarioProjection projection;
            projection.entryId = entry.id;
            projection.name = entry.name;
            if (resolution) projection.matchState = resolution->state;
            if (!history.empty()) {
                projection.runCount = static_cast<int>(history.size());
                const RunFact *bestRun = history.front();
                double playtime = 0.0;
                for (const auto *fact: history) {
                    if (fact->score > bestRun->score ||
                        (fact->score == bestRun->score && fact->run_id.start_time < bestRun->run_id.start_time))
                        bestRun = fact;
                    playtime += fact->duration_seconds;
                }
                projection.personalBest = static_cast<double>(bestRun->score);
                projection.personalBestTime = bestRun->run_id.startSecond();
                projection.playtimeSeconds = playtime;
                const auto sample = std::min(kRecentRuns, history.size());
                double total = 0.0;
                for (std::size_t index = history.size() - sample; index < history.size(); ++index)
                    total += history[index]->score;
                projection.recentAverage = total / static_cast<double>(sample);
                projection.recentSampleCount = static_cast<int>(sample);
            }
            projection.normalizedRank = normalizedFromLadder(ladder, projection.personalBest);
            if (!ladder) return projection;
            if (!projection.personalBest) {
                // Unplayed attains nothing even when the first threshold is 0, which is what keeps
                // "never played" and "played and scored 0" distinguishable -- both normalize to 0.
                projection.nextThreshold = Threshold{tiers.front().id, ladder->front()};
                return projection;
            }
            for (std::size_t index = 0; index < ladder->size(); ++index) {
                if (*projection.personalBest >= (*ladder)[index]) projection.attainedTier = tiers[index].id;
                else {
                    projection.nextThreshold = Threshold{tiers[index].id, (*ladder)[index]};
                    break;
                }
            }
            return projection;
        }

        double sumOf(const std::vector<ScenarioProjection> &scenarios, double ScenarioProjection::*field) {
            double total = 0.0;
            for (const auto &scenario: scenarios) total += scenario.*field;
            return total;
        }

        // ---- Run indexing ----------------------------------------------------------------------

        struct RunIndex {
            EntriesByHash entriesByHash; // Resolved entries only
            std::vector<History> histories;
        };

        // Both members index entries in collectEntries order. runFacts is chronological, so each
        // history is too.
        RunIndex indexRuns(const BenchmarkDefinition &definition, const ResolutionByEntry &resolutions,
                           const std::vector<RunFact> &runFacts) {
            const auto ordered = collectEntries(definition);
            RunIndex index{{}, std::vector<History>(ordered.size())};
            for (std::size_t entry = 0; entry < ordered.size(); ++entry) {
                const auto *resolution = findResolution(resolutions, ordered[entry]->id);
                if (!resolution || resolution->state != ScenarioMatchState::Resolved) continue;
                for (const auto &hash: ordered[entry]->hashes) index.entriesByHash[hash].push_back(entry);
            }
            for (const auto &fact: runFacts)
                if (const auto bucket = index.entriesByHash.find(fact.run_id.scenario_id.hash);
                    bucket != index.entriesByHash.end())
                    for (const auto entry: bucket->second) index.histories[entry].push_back(&fact);
            return index;
        }

        // ---- Scenario tree ---------------------------------------------------------------------

        struct EntryState {
            ScenarioEntryId id;
            std::optional<std::vector<double> > ladder;
            std::optional<double> personalBest;

            [[nodiscard]] bool hasTier(const std::size_t tier) const { return ladder && tier < ladder->size(); }

            [[nodiscard]] double thresholdAt(const std::size_t tier) const {
                return hasTier(tier) ? (*ladder)[tier] : 0.0;
            }

            [[nodiscard]] bool attains(const std::size_t tier) const {
                return personalBest && hasTier(tier) && *personalBest >= (*ladder)[tier];
            }

            [[nodiscard]] double normalized(const double score) const { return normalizedFromLadder(ladder, score); }
        };

        struct StateRange {
            std::size_t begin = 0;
            std::size_t end = 0;
        };

        // Met when any one of its scenarios attains the tier.
        struct Requirement {
            GroupId group;
            StateRange scenarios;
        };

        // A category is satisfied when every requirement is: one per subcategory when it has any,
        // otherwise its direct scenarios as a single requirement. In the first case the requirements
        // are index-parallel with the category's GroupProjection::subgroups.
        struct CategoryLayout {
            std::vector<Requirement> requirements;
        };

        // `states` is index-parallel with BenchmarkProjection::scenarios, and every range indexes it,
        // so the rank rules never look an entry up by id.
        struct ScenarioTree {
            std::vector<EntryState> states;
            StateRange uncategorized;
            std::vector<CategoryLayout> categories;
        };

        // Fills the scenario and group outputs of `projection` alongside the tree describing them.
        ScenarioTree buildScenarioTree(const BenchmarkDefinition &definition, const ResolutionByEntry &resolutions,
                                       const std::vector<History> &histories, BenchmarkProjection &projection) {
            ScenarioTree tree;
            tree.states.reserve(histories.size());
            const auto append = [&](const std::vector<ScenarioEntry> &entries, std::vector<ScenarioEntryId> &target) {
                const auto begin = tree.states.size();
                for (const auto &entry: entries) {
                    auto ladder = thresholdLadder(definition.tiers, entry.thresholds);
                    projection.scenarios.push_back(projectScenario(entry, definition.tiers,
                                                                   findResolution(resolutions, entry.id),
                                                                   histories[tree.states.size()], ladder));
                    tree.states.push_back({entry.id, std::move(ladder), projection.scenarios.back().personalBest});
                    target.push_back(entry.id);
                }
                return StateRange{begin, tree.states.size()};
            };

            tree.uncategorized = append(definition.uncategorized, projection.uncategorized);
            for (const auto &category: definition.categories) {
                GroupProjection group{category.id, category.name, std::nullopt, {}, {}};
                CategoryLayout layout;
                const auto direct = append(category.scenarios, group.scenarios);
                for (const auto &subcategory: category.subcategories) {
                    GroupProjection subgroup{subcategory.id, subcategory.name, std::nullopt, {}, {}};
                    layout.requirements.push_back({subcategory.id, append(subcategory.scenarios, subgroup.scenarios)});
                    group.subgroups.push_back(std::move(subgroup));
                }
                if (category.subcategories.empty()) layout.requirements.push_back({category.id, direct});
                projection.categories.push_back(std::move(group));
                tree.categories.push_back(std::move(layout));
            }
            return tree;
        }

        // ---- Tier rules ------------------------------------------------------------------------

        struct TierRules {
            const std::vector<Tier> &tiers;
            const ScenarioTree &tree;

            [[nodiscard]] std::span<const EntryState> slice(const StateRange range) const {
                return std::span(tree.states).subspan(range.begin, range.end - range.begin);
            }

            [[nodiscard]] bool met(const Requirement &requirement, const std::size_t tier) const {
                return std::ranges::any_of(slice(requirement.scenarios),
                                           [&](const EntryState &state) { return state.attains(tier); });
            }

            [[nodiscard]] bool satisfied(const CategoryLayout &category, const std::size_t tier) const {
                return std::ranges::all_of(category.requirements,
                                           [&](const Requirement &requirement) { return met(requirement, tier); });
            }

            [[nodiscard]] bool allAttain(const std::span<const EntryState> states, const std::size_t tier) const {
                return std::ranges::all_of(states, [&](const EntryState &state) { return state.attains(tier); });
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

        void assignGroupTiers(const TierRules &rules, BenchmarkProjection &projection) {
            for (std::size_t categoryIndex = 0; categoryIndex < projection.categories.size(); ++categoryIndex) {
                auto &group = projection.categories[categoryIndex];
                const auto &layout = rules.tree.categories[categoryIndex];
                group.satisfiedTier = rules.highestTier([&](const std::size_t tier) {
                    return rules.satisfied(layout, tier);
                });
                for (std::size_t subgroupIndex = 0; subgroupIndex < group.subgroups.size(); ++subgroupIndex)
                    group.subgroups[subgroupIndex].satisfiedTier = rules.highestTier([&](const std::size_t tier) {
                        return rules.met(layout.requirements[subgroupIndex], tier);
                    });
            }
        }

        void collectBlockers(const std::span<const EntryState> states, const std::optional<GroupId> &group,
                             const std::size_t tier, std::vector<RankBlocker> &blockers) {
            for (const auto &state: states)
                if (!state.attains(tier))
                    blockers.push_back({state.id, group, state.thresholdAt(tier), state.personalBest});
        }

        void assignNextTier(const TierRules &rules, const std::size_t tier, BenchmarkProjection &projection) {
            projection.nextTier = rules.tiers[tier].id;
            projection.scenariosAtNextTier = static_cast<int>(std::ranges::count_if(
                rules.tree.states, [&](const EntryState &state) { return state.attains(tier); }));
            collectBlockers(rules.slice(rules.tree.uncategorized), std::nullopt, tier, projection.nextTierBlockers);
            for (const auto &category: rules.tree.categories)
                for (const auto &requirement: category.requirements)
                    if (!rules.met(requirement, tier))
                        collectBlockers(rules.slice(requirement.scenarios), requirement.group, tier,
                                        projection.nextTierBlockers);
        }

        // ---- Rank history ----------------------------------------------------------------------

        struct EntrySweep {
            std::optional<double> best;
            double bestNormalized = 0.0;
            std::deque<double> recent;
            double recentNormalized = 0.0;

            /** @return Change in the normalized recent average. */
            double pushRecent(const double score, const EntryState &state) {
                recent.push_back(score);
                if (recent.size() > kRecentRuns) recent.pop_front();
                // Summed oldest first, as projectScenario does, so the last point equals its average.
                const double total = std::accumulate(recent.begin(), recent.end(), 0.0);
                const double updated = state.normalized(total / static_cast<double>(recent.size()));
                return updated - std::exchange(recentNormalized, updated);
            }

            /** @return Change in the normalized best, or nothing when `score` is not a new best. */
            std::optional<double> offerBest(const double score, const EntryState &state) {
                if (best && score <= *best) return std::nullopt;
                best = score;
                const double updated = state.normalized(score);
                return updated - std::exchange(bestNormalized, updated);
            }
        };

        // Running sums of every entry's normalized value rather than re-averaging per point, so the
        // sweep stays linear in runs plus emitted points. runFacts is chronological by contract
        // (UserProfile::getRunFacts), so each day is one contiguous block and no day is revisited.
        void sweepRankHistory(const std::vector<RunFact> &runFacts, const EntriesByHash &entriesByHash,
                              const std::vector<EntryState> &states, BenchmarkProjection &projection) {
            std::vector<EntrySweep> sweeps(states.size());
            double bestRunning = 0.0;
            double recentRunning = 0.0;
            const auto entryCount = static_cast<double>(states.size());
            for (std::size_t index = 0; index < runFacts.size();) {
                const auto day = runFacts[index].run_id.startDay();
                bool played = false;
                bool improved = false;
                for (; index < runFacts.size() && runFacts[index].run_id.startDay() == day; ++index) {
                    const auto bucket = entriesByHash.find(runFacts[index].run_id.scenario_id.hash);
                    if (bucket == entriesByHash.end()) continue;
                    played = true;
                    const auto score = static_cast<double>(runFacts[index].score);
                    for (const auto entry: bucket->second) {
                        recentRunning += sweeps[entry].pushRecent(score, states[entry]);
                        if (const auto delta = sweeps[entry].offerBest(score, states[entry])) {
                            bestRunning += *delta;
                            improved = true;
                        }
                    }
                }
                if (improved) projection.averageRankHistory.push_back({day, bestRunning / entryCount});
                if (played) projection.recentAverageRankHistory.push_back({day, recentRunning / entryCount});
            }
        }
    }

    BenchmarkProjection evaluateBenchmark(
        const BenchmarkDefinition &definition, const CompletenessResult &completeness,
        const std::vector<ScenarioResolution> &resolutions, const std::vector<RunFact> &runFacts,
        std::vector<std::pair<std::chrono::sys_days, double> > rollingPlaytime) {
        BenchmarkProjection projection{
            .benchmarkId = definition.id,
            .completeness = completeness.completeness,
            .tiers = definition.tiers,
            .rollingPlaytime = std::move(rollingPlaytime),
        };
        ResolutionByEntry byEntry;
        for (const auto &resolution: resolutions) byEntry.emplace(resolution.entryId.value, &resolution);

        // Built for Incomplete definitions too: per-entry stats precede the Trackable gate.
        const auto runs = indexRuns(definition, byEntry, runFacts);
        const auto tree = buildScenarioTree(definition, byEntry, runs.histories, projection);
        projection.totalPlaytimeSeconds = sumOf(projection.scenarios, &ScenarioProjection::playtimeSeconds);

        // Everything below is Trackable-only: official rank, completion, blockers and average-rank
        // outputs stay at their defaults for an Incomplete definition, so partial thresholds can
        // never produce a plausible-looking rank. The emptiness half also keeps averageRank's
        // division defined.
        if (completeness.completeness != Completeness::Trackable || projection.scenarios.empty())
            return projection;

        const TierRules rules{definition.tiers, tree};
        const auto attained = rules.highestIndex([&](const std::size_t tier) { return rules.rankAttained(tier); });
        if (attained) projection.attainedRank = definition.tiers[*attained].id;
        projection.completedRank = rules.highestTier([&](const std::size_t tier) { return rules.completed(tier); });
        assignGroupTiers(rules, projection);
        projection.averageRank = sumOf(projection.scenarios, &ScenarioProjection::normalizedRank) /
                                 static_cast<double>(projection.scenarios.size());

        if (const std::size_t next = attained ? *attained + 1 : 0; next < definition.tiers.size())
            assignNextTier(rules, next, projection);

        sweepRankHistory(runFacts, runs.entriesByHash, tree.states, projection);
        return projection;
    }
}
