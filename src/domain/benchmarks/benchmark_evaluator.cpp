#include "benchmarks/benchmark_evaluator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <functional>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>

namespace ksv::domain {
    namespace {
        double normalizedFromLadder(const std::vector<double> &ladder, const std::optional<double> score) {
            if (!score || !std::isfinite(*score)) return 0.0;
            const double value = *score;
            if (value < ladder.front())
                return ladder.front() > 0.0 ? std::max(0.0, value / ladder.front()) : 0.0;
            for (std::size_t index = 1; index < ladder.size(); ++index)
                if (value < ladder[index])
                    return static_cast<double>(index) +
                           (value - ladder[index - 1]) / (ladder[index] - ladder[index - 1]);
            return static_cast<double>(ladder.size());
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
        const auto ladder = thresholdLadder(tiers, thresholds);
        return ladder ? normalizedFromLadder(*ladder, score) : 0.0;
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
        std::vector<const ScenarioEntry *> collectEntries(const Benchmark &definition) {
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
            projection.normalizedRank = ladder ? normalizedFromLadder(*ladder, projection.personalBest) : 0.0;
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

        struct EntryState {
            std::optional<std::vector<double> > ladder;
            std::optional<double> personalBest;
        };

        // `states` is index-parallel with BenchmarkProjection::scenarios, so the rank rules need no
        // id lookup on the hot path.
        struct ProjectionBuilder {
            const std::vector<Tier> &tiers;
            const ResolutionByEntry &resolutions;
            const std::vector<History> &histories;
            BenchmarkProjection &projection;
            std::vector<EntryState> states;

            void append(const std::vector<ScenarioEntry> &entries, std::vector<ScenarioEntryId> &target) {
                for (const auto &entry: entries) {
                    const auto index = projection.scenarios.size();
                    auto ladder = thresholdLadder(tiers, entry.thresholds);
                    projection.scenarios.push_back(projectScenario(
                        entry, tiers, findResolution(resolutions, entry.id), histories[index], ladder));
                    states.push_back({std::move(ladder), projection.scenarios.back().personalBest});
                    target.push_back(entry.id);
                }
            }
        };

        bool attains(const EntryState &state, const std::size_t tierIndex) {
            return state.personalBest && state.ladder && tierIndex < state.ladder->size() &&
                   *state.personalBest >= (*state.ladder)[tierIndex];
        }

        using IndexById = std::unordered_map<std::string, std::size_t>;

        bool anyAttains(const std::vector<ScenarioEntry> &entries, const std::vector<EntryState> &states,
                        const IndexById &indexById, const std::size_t tierIndex) {
            return std::ranges::any_of(entries, [&](const ScenarioEntry &entry) {
                return attains(states[indexById.at(entry.id.value)], tierIndex);
            });
        }

        bool allAttain(const std::vector<ScenarioEntry> &entries, const std::vector<EntryState> &states,
                       const IndexById &indexById, const std::size_t tierIndex) {
            return !entries.empty() && std::ranges::all_of(entries, [&](const ScenarioEntry &entry) {
                return attains(states[indexById.at(entry.id.value)], tierIndex);
            });
        }

        bool categorySatisfied(const Category &category, const std::vector<EntryState> &states,
                               const IndexById &indexById, const std::size_t tierIndex) {
            if (!category.subcategories.empty())
                return std::ranges::all_of(category.subcategories, [&](const Subcategory &subcategory) {
                    return anyAttains(subcategory.scenarios, states, indexById, tierIndex);
                });
            return anyAttains(category.scenarios, states, indexById, tierIndex);
        }

        bool attainedAtTier(const Benchmark &definition, const std::vector<EntryState> &states,
                            const IndexById &indexById, const std::size_t tierIndex) {
            return (definition.uncategorized.empty() ||
                    allAttain(definition.uncategorized, states, indexById, tierIndex)) &&
                   std::ranges::all_of(definition.categories, [&](const Category &category) {
                       return categorySatisfied(category, states, indexById, tierIndex);
                   });
        }

        std::optional<std::size_t> highestSatisfied(const std::size_t tierCount,
                                                    const std::function<bool(std::size_t)> &satisfied) {
            for (std::size_t index = tierCount; index-- > 0;)
                if (satisfied(index)) return index;
            return std::nullopt;
        }

        void collectBlockers(const std::vector<ScenarioEntry> &entries, const std::optional<GroupId> &group,
                             const std::vector<EntryState> &states, const IndexById &indexById,
                             const std::size_t tierIndex, std::vector<RankBlocker> &blockers) {
            for (const auto &entry: entries) {
                const auto &state = states[indexById.at(entry.id.value)];
                if (attains(state, tierIndex)) continue;
                blockers.push_back({entry.id, group,
                                    state.ladder && tierIndex < state.ladder->size()
                                        ? (*state.ladder)[tierIndex] : 0.0,
                                    state.personalBest});
            }
        }
    }

    BenchmarkProjection evaluateBenchmark(
        const Benchmark &definition, const CompletenessResult &completeness,
        const std::vector<ScenarioResolution> &resolutions, const std::vector<RunFact> &runFacts,
        std::vector<std::pair<std::chrono::sys_days, double> > rollingPlaytime) {
        BenchmarkProjection projection;
        projection.benchmarkId = definition.id;
        projection.completeness = completeness.completeness;
        projection.tiers = definition.tiers;
        projection.rollingPlaytime = std::move(rollingPlaytime);
        ResolutionByEntry byEntry;
        for (const auto &resolution: resolutions) byEntry.emplace(resolution.entryId.value, &resolution);

        // Built for Incomplete definitions too: per-entry stats precede the Trackable gate.
        const auto ordered = collectEntries(definition);
        EntriesByHash entriesByHash;
        for (std::size_t index = 0; index < ordered.size(); ++index) {
            const auto *resolution = findResolution(byEntry, ordered[index]->id);
            if (!resolution || resolution->state != ScenarioMatchState::Resolved) continue;
            for (const auto &hash: ordered[index]->hashes) entriesByHash[hash].push_back(index);
        }
        // runFacts is chronological, so each history is too.
        std::vector<History> histories(ordered.size());
        for (const auto &fact: runFacts)
            if (const auto bucket = entriesByHash.find(fact.run_id.scenario_id.hash); bucket != entriesByHash.end())
                for (const auto index: bucket->second) histories[index].push_back(&fact);

        ProjectionBuilder builder{definition.tiers, byEntry, histories, projection, {}};
        builder.states.reserve(ordered.size());
        builder.append(definition.uncategorized, projection.uncategorized);
        for (const auto &category: definition.categories) {
            GroupProjection group{category.id, category.name, std::nullopt, {}, {}};
            builder.append(category.scenarios, group.scenarios);
            for (const auto &subcategory: category.subcategories) {
                GroupProjection subgroup{subcategory.id, subcategory.name, std::nullopt, {}, {}};
                builder.append(subcategory.scenarios, subgroup.scenarios);
                group.subgroups.push_back(std::move(subgroup));
            }
            projection.categories.push_back(std::move(group));
        }
        for (const auto &scenario: projection.scenarios) projection.totalPlaytimeSeconds += scenario.playtimeSeconds;
        const auto &states = builder.states;

        // Everything below is Trackable-only: official rank, completion, blockers and average-rank
        // outputs stay at their defaults for an Incomplete definition, so partial thresholds can
        // never produce a plausible-looking rank. The emptiness half also keeps averageRank's
        // division defined.
        if (completeness.completeness != Completeness::Trackable || projection.scenarios.empty())
            return projection;

        IndexById indexById;
        for (std::size_t index = 0; index < projection.scenarios.size(); ++index)
            indexById.emplace(projection.scenarios[index].entryId.value, index);
        const std::size_t tierCount = definition.tiers.size();
        const auto attainedIndex = highestSatisfied(tierCount, [&](const std::size_t tier) {
            return attainedAtTier(definition, states, indexById, tier);
        });
        if (attainedIndex) projection.attainedRank = definition.tiers[*attainedIndex].id;

        if (const auto completed = highestSatisfied(tierCount, [&](const std::size_t tier) {
            return std::ranges::all_of(states, [&](const EntryState &state) { return attains(state, tier); });
        }))
            projection.completedRank = definition.tiers[*completed].id;

        for (std::size_t categoryIndex = 0; categoryIndex < projection.categories.size(); ++categoryIndex) {
            auto &group = projection.categories[categoryIndex];
            const auto &category = definition.categories[categoryIndex];
            if (const auto satisfied = highestSatisfied(tierCount, [&](const std::size_t tier) {
                return categorySatisfied(category, states, indexById, tier);
            }))
                group.satisfiedTier = definition.tiers[*satisfied].id;
            for (std::size_t subgroupIndex = 0; subgroupIndex < group.subgroups.size(); ++subgroupIndex) {
                const auto &subcategory = category.subcategories[subgroupIndex];
                if (const auto satisfied = highestSatisfied(tierCount, [&](const std::size_t tier) {
                    return anyAttains(subcategory.scenarios, states, indexById, tier);
                }))
                    group.subgroups[subgroupIndex].satisfiedTier = definition.tiers[*satisfied].id;
            }
        }

        double normalizedSum = 0.0;
        for (const auto &scenario: projection.scenarios) normalizedSum += scenario.normalizedRank;
        projection.averageRank = normalizedSum / static_cast<double>(projection.scenarios.size());

        const std::size_t nextIndex = attainedIndex ? *attainedIndex + 1 : 0;
        if (nextIndex < tierCount) {
            projection.nextTier = definition.tiers[nextIndex].id;
            for (const auto &state: states)
                if (attains(state, nextIndex)) ++projection.scenariosAtNextTier;
            collectBlockers(definition.uncategorized, std::nullopt, states, indexById, nextIndex,
                            projection.nextTierBlockers);
            for (const auto &category: definition.categories) {
                if (categorySatisfied(category, states, indexById, nextIndex)) continue;
                if (category.subcategories.empty()) {
                    collectBlockers(category.scenarios, category.id, states, indexById, nextIndex,
                                    projection.nextTierBlockers);
                    continue;
                }
                for (const auto &subcategory: category.subcategories) {
                    if (anyAttains(subcategory.scenarios, states, indexById, nextIndex)) continue;
                    collectBlockers(subcategory.scenarios, subcategory.id, states, indexById, nextIndex,
                                    projection.nextTierBlockers);
                }
            }
        }

        // Running sums of every entry's normalized value rather than re-averaging per point, so the
        // sweep stays linear in runs plus emitted points. runFacts is chronological by contract
        // (UserProfile::getRunFacts), so each day is one contiguous block and no day is revisited.
        const auto normalizedOf = [&](const std::size_t scenarioIndex, const double score) {
            return states[scenarioIndex].ladder ? normalizedFromLadder(*states[scenarioIndex].ladder, score) : 0.0;
        };
        struct EntrySweep {
            std::optional<double> best;
            double bestNormalized = 0.0;
            std::deque<double> recent;
            double recentNormalized = 0.0;
        };
        std::vector<EntrySweep> sweeps(projection.scenarios.size());
        double normalizedRunning = 0.0;
        double recentRunning = 0.0;
        const auto entryCount = static_cast<double>(projection.scenarios.size());
        for (std::size_t index = 0; index < runFacts.size();) {
            const auto day = runFacts[index].run_id.startDay();
            bool played = false;
            bool improved = false;
            for (; index < runFacts.size() && runFacts[index].run_id.startDay() == day; ++index) {
                const auto bucket = entriesByHash.find(runFacts[index].run_id.scenario_id.hash);
                if (bucket == entriesByHash.end()) continue;
                played = true;
                const auto score = static_cast<double>(runFacts[index].score);
                for (const auto scenarioIndex: bucket->second) {
                    auto &sweep = sweeps[scenarioIndex];
                    sweep.recent.push_back(score);
                    if (sweep.recent.size() > kRecentRuns) sweep.recent.pop_front();
                    // Summed oldest first, as projectScenario does, so the last point equals its average.
                    const double total = std::accumulate(sweep.recent.begin(), sweep.recent.end(), 0.0);
                    const double recentUpdated =
                        normalizedOf(scenarioIndex, total / static_cast<double>(sweep.recent.size()));
                    recentRunning += recentUpdated - std::exchange(sweep.recentNormalized, recentUpdated);

                    if (sweep.best && score <= *sweep.best) continue;
                    sweep.best = score;
                    const double updated = normalizedOf(scenarioIndex, score);
                    normalizedRunning += updated - std::exchange(sweep.bestNormalized, updated);
                    improved = true;
                }
            }
            if (improved) projection.averageRankHistory.push_back({day, normalizedRunning / entryCount});
            if (played) projection.recentAverageRankHistory.push_back({day, recentRunning / entryCount});
        }
        return projection;
    }
}
