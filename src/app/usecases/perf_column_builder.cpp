//
// Created by Lecka on 08/08/2026.
//

#include "perf_column_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace ksv::application {
    namespace {
        // The data from the perf grouped into 1s Buckets
        struct Bucket {
            float shots = 0.0F;
            float hits = 0.0F;
            float kills = 0.0F;
            float dmg = 0.0F;
            float score = 0.0F;
        };

        // What a column definition sees: the whole run, not just one instant.
        // Needed for anything that isn't a pure per-second value - a running
        // total, or a projection that extrapolates from the pace-so-far (or a
        // trailing window of it) out to the run's actual duration.
        struct BuildContext {
            const domain::ScenarioPerf &perf;
            const std::vector<Bucket> &buckets;
        };

        struct ColumnDefinition {
            ColumnId id;
            std::function<std::vector<float>(const BuildContext &)> derive;
        };

        // Convenience for the common case: a column whose value at second i
        // depends only on bucket i.
        std::vector<float> perBucket(const std::vector<Bucket> &buckets, const std::function<float(const Bucket &)> &f) {
            std::vector<float> result(buckets.size());
            for (size_t i = 0; i < buckets.size(); ++i) result[i] = f(buckets[i]);
            return result;
        }

        const std::array<ColumnDefinition, 8> kColumnDefinitions{{
            {
                ColumnId::Score,
                [](const BuildContext &ctx) { return perBucket(ctx.buckets, [](const Bucket &b) { return b.score; }); }
            },
            {
                ColumnId::Accuracy,
                [](const BuildContext &ctx) {
                    return perBucket(ctx.buckets, [](const Bucket &b) { return b.shots > 0.0F ? b.hits / b.shots : 0.0F; });
                }
            },
            {
                ColumnId::Shots,
                [](const BuildContext &ctx) { return perBucket(ctx.buckets, [](const Bucket &b) { return b.shots; }); }
            },
            {
                ColumnId::Kills,
                [](const BuildContext &ctx) { return perBucket(ctx.buckets, [](const Bucket &b) { return b.kills; }); }
            },
            {
                ColumnId::Dmg,
                [](const BuildContext &ctx) { return perBucket(ctx.buckets, [](const Bucket &b) { return b.dmg; }); }
            },
            {
                ColumnId::ScoreTotal,
                [](const BuildContext &ctx) {
                    std::vector<float> result(ctx.buckets.size());
                    float running = 0.0F;
                    for (size_t i = 0; i < ctx.buckets.size(); ++i) {
                        running += ctx.buckets[i].score;
                        result[i] = running;
                    }
                    return result;
                }
            },
            {
                // Projects the final score by extrapolating the average pace
                // so far (cumulative score / elapsed time) across the run's
                // full duration.
                ColumnId::ExpectedFinalScore,
                [](const BuildContext &ctx) {
                    std::vector<float> result(ctx.buckets.size());
                    const float totalDuration = ctx.perf.scenario_length;
                    float running = 0.0F;
                    for (size_t i = 0; i < ctx.buckets.size(); ++i) {
                        running += ctx.buckets[i].score;
                        const float elapsed = float(i) + 1.0F;
                        result[i] = totalDuration > 0.0F ? running / elapsed * totalDuration : running;
                    }
                    return result;
                }
            },
            {
                // Same projection, but paced off only the trailing 5 seconds
                // rather than the whole run so far - reacts to a recent
                // hot/cold streak instead of averaging it away.
                ColumnId::ExpectedFinalScoreRecent,
                [](const BuildContext &ctx) {
                    constexpr size_t kWindowSeconds = 5;
                    std::vector<float> result(ctx.buckets.size());
                    const float totalDuration = ctx.perf.scenario_length;
                    for (size_t i = 0; i < ctx.buckets.size(); ++i) {
                        const size_t windowStart = i + 1 >= kWindowSeconds ? i + 1 - kWindowSeconds : 0;
                        float windowScore = 0.0F;
                        for (size_t j = windowStart; j <= i; ++j) windowScore += ctx.buckets[j].score;
                        const float windowLength = float(i - windowStart + 1);
                        const float pace = windowScore / windowLength;
                        result[i] = totalDuration > 0.0F ? pace * totalDuration : windowScore;
                    }
                    return result;
                }
            },
        }};
    }

    GraphSeries PerfColumnBuilder::build(const domain::ScenarioPerf &perf) {
        GraphSeries result;
        if (perf.data.empty()) return result;

        int maxSecond = 0;
        for (const auto &point: perf.data) maxSecond = std::max(maxSecond, int(std::lround(point.time)));

        std::vector<Bucket> buckets(maxSecond + 1);
        for (const auto &point: perf.data) {
            const int bucket = std::clamp(int(std::lround(point.time)), 0, maxSecond);
            buckets[bucket].shots += float(point.shots);
            buckets[bucket].hits += float(point.hits);
            buckets[bucket].kills += float(point.kills);
            buckets[bucket].dmg += point.dmg;
            buckets[bucket].score += point.score;
        }

        result.times.resize(maxSecond + 1);
        for (int s = 0; s <= maxSecond; ++s) result.times[s] = float(s);

        const BuildContext context{perf, buckets};
        for (const auto &def: kColumnDefinitions) result.columns[def.id] = def.derive(context);

        return result;
    }
}
