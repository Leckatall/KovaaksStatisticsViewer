#include "average_line_use_case.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "bucketed_run.h"

namespace ksv::application {
    namespace {
        using Values = std::vector<double>;

        bool finite(const Values &values) {
            return std::ranges::all_of(values, [](const double value) { return std::isfinite(value); });
        }

        std::optional<Values> evaluateNode(const domain::Run &, const BucketedRun &, const Expression &,
                                           const IProfileService &, size_t graphLength);

        std::optional<Values> evaluateAverage(const domain::Run &reference,
                                              const BucketedRun &referenceBuckets,
                                              const AverageAcrossRuns &node, const IProfileService &profiles,
                                              const size_t graphLength) {
            const auto referenceValues = evaluateNode(reference, referenceBuckets, node.input, profiles, graphLength);
            if (!referenceValues) return std::nullopt;
            struct Candidate {
                domain::Run perf;
                Values values;
            };
            std::vector<Candidate> candidates;
            for (const auto &candidate: profiles.getRunsForScenario(reference.run_id.scenario_id)) {
                if (candidate.run_id == reference.run_id) continue;
                const auto values = evaluateNode(candidate, bucketRun(candidate), node.input, profiles, graphLength);
                if (values && values->size() == referenceValues->size() && finite(*values))
                    candidates.push_back({
                        candidate, *values
                    });
            }
            std::visit([&](const auto &selection) {
                using Selection = std::decay_t<decltype(selection)>;
                if constexpr (std::same_as<Selection, RecentRuns>) {
                    std::ranges::sort(candidates, [](const Candidate &left, const Candidate &right) {
                        return left.perf.run_id.start_time != right.perf.run_id.start_time
                                   ? left.perf.run_id.start_time > right.perf.run_id.start_time
                                   : right.perf.run_id < left.perf.run_id;
                    });
                    if (candidates.size() > selection.count) candidates.resize(selection.count);
                } else {
                    std::ranges::stable_sort(candidates, [](const Candidate &left, const Candidate &right) {
                        return left.perf.totals().score > right.perf.totals().score;
                    });
                    const auto count = static_cast<size_t>(std::ceil(selection.percent * candidates.size() / 100.0));
                    if (count < candidates.size()) {
                        const auto cutoff = candidates[count - 1].perf.totals().score;
                        const auto end = std::ranges::find_if(candidates, [cutoff](const Candidate &candidate) {
                            return candidate.perf.totals().score < cutoff;
                        });
                        candidates.erase(end, candidates.end());
                    }
                }
            }, node.selection);
            if (candidates.size() < 2) return std::nullopt;
            Values result(referenceValues->size());
            for (const auto &candidate: candidates)
                for (size_t i = 0; i < result.size(); ++i) result[i] += candidate.values[i];
            for (auto &value: result) value /= static_cast<double>(candidates.size());
            return finite(result) ? std::optional{std::move(result)} : std::nullopt;
        }

        std::optional<Values> evaluateNode(const domain::Run &perf, const BucketedRun &buckets,
                                           const Expression &expression, const IProfileService &profiles,
                                           const size_t graphLength) {
            if (!expression) return std::nullopt;
            return std::visit([&](const auto &node) -> std::optional<Values> {
                using Node = std::decay_t<decltype(node)>;
                if constexpr (std::same_as<Node, PrimitiveReference>) return buckets.valuesFor(node.metric);
                else if constexpr (std::same_as<Node, NumericConstant>) return Values(graphLength, node.value);
                else if constexpr (std::same_as<Node, Add> || std::same_as<Node, Subtract> || std::same_as<Node,
                                       Multiply> || std::same_as<Node, Divide>) {
                    const auto left = evaluateNode(perf, buckets, node.left, profiles, graphLength);
                    const auto right = evaluateNode(perf, buckets, node.right, profiles, graphLength);
                    if (!left || !right || left->size() != right->size()) return std::nullopt;
                    Values result(left->size());
                    for (size_t i = 0; i < result.size(); ++i) {
                        if constexpr (std::same_as<Node, Add>) result[i] = (*left)[i] + (*right)[i];
                        else if constexpr (std::same_as<Node, Subtract>) result[i] = (*left)[i] - (*right)[i];
                        else if constexpr (std::same_as<Node, Multiply>) result[i] = (*left)[i] * (*right)[i];
                        else result[i] = (*right)[i] == 0.0 ? 0.0 : (*left)[i] / (*right)[i];
                    }
                    return finite(result) ? std::optional{std::move(result)} : std::nullopt;
                } else if constexpr (std::same_as<Node, RunningSum>) {
                    auto values = evaluateNode(perf, buckets, node.input, profiles, graphLength);
                    if (!values) return std::nullopt;
                    for (size_t i = 1; i < values->size(); ++i) (*values)[i] += (*values)[i - 1];
                    return finite(*values) ? values : std::nullopt;
                } else if constexpr (std::same_as<Node, RollingMean>) {
                    const auto input = evaluateNode(perf, buckets, node.input, profiles, graphLength);
                    if (!input) return std::nullopt;
                    Values result(input->size());
                    double sum = 0.0;
                    for (size_t i = 0; i < result.size(); ++i) {
                        sum += (*input)[i];
                        if (i >= node.window) sum -= (*input)[i - node.window];
                        result[i] = sum / std::min<size_t>(i + 1, node.window);
                    }
                    return finite(result) ? std::optional{std::move(result)} : std::nullopt;
                } else if constexpr (std::same_as<Node, ProjectedFinalValue> || std::same_as<Node,
                                         ProjectRateToFinal>) {
                    auto values = evaluateNode(perf, buckets, node.input, profiles, graphLength);
                    if (!values) return std::nullopt;
                    for (size_t i = 0; i < values->size(); ++i) {
                        if constexpr (std::same_as<Node, ProjectedFinalValue>)
                            (*values)[i] = (*values)[i] / static_cast<double>(i + 1) * values->size();
                        else (*values)[i] *= values->size();
                    }
                    return finite(*values) ? values : std::nullopt;
                } else if constexpr (std::same_as<Node, FinalValue>) {
                    const auto &totals = perf.totals();
                    switch (node.stat) {
                        case FinalStat::Score: return Values(graphLength, totals.score);
                        case FinalStat::Shots: return Values(graphLength, totals.shots);
                        case FinalStat::Hits: return Values(graphLength, totals.hits);
                        case FinalStat::Misses: return Values(graphLength, totals.misses);
                        case FinalStat::Kills: return Values(graphLength, totals.kills);
                        case FinalStat::Dmg:
                            if (!totals.damage) return std::nullopt;
                            return Values(graphLength, *totals.damage);
                    }
                    return std::nullopt;
                } else if constexpr (std::same_as<Node, BestRun>) {
                    const domain::Run *best = nullptr;
                    const auto runs = profiles.getRunsForScenario(perf.run_id.scenario_id);
                    for (const auto &candidate: runs) {
                        if (candidate.run_id == perf.run_id) continue;
                        if (!best || candidate.totals().score > best->totals().score ||
                            (candidate.totals().score == best->totals().score &&
                             candidate.run_id.start_time < best->run_id.start_time))
                            best = &candidate;
                    }
                    if (!best) return std::nullopt;
                    return evaluateNode(*best, bucketRun(*best), node.input, profiles, graphLength);
                } else return evaluateAverage(perf, buckets, node, profiles, graphLength);
            }, expression->value());
        }
    }

    AverageLineUseCase::AverageLineUseCase(std::shared_ptr<IProfileService> profileService) : m_profileService(
        std::move(profileService)) {
    }

    std::optional<std::vector<double> > AverageLineUseCase::evaluate(const domain::Run &referenceRun,
                                                                     const Expression &expression) const {
        auto referenceBuckets = bucketRun(referenceRun);
        const auto graphLength = referenceBuckets.times.size();
        return evaluateNode(referenceRun, referenceBuckets, expression, *m_profileService, graphLength);
    }
}
