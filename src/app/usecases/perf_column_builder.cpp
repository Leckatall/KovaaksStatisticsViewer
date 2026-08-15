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
        struct Bucket {
            float shots = 0.0F;
            float hits = 0.0F;
            float kills = 0.0F;
            float dmg = 0.0F;
            float score = 0.0F;
        };

        struct BuildContext {
            const domain::ScenarioPerf &perf;
            const std::vector<Bucket> &buckets;
        };

        struct ColumnDefinition {
            ColumnId id;
            std::function<std::vector<float>(const BuildContext &)> derive;
        };

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
                // Extrapolates final score from average pace-so-far across the run's observed
                // duration (bucket count), not the game-reported scenario_length: scenarios that
                // manipulate time flow report a scaled duration that doesn't match real elapsed time.
                ColumnId::ExpectedFinalScore,
                [](const BuildContext &ctx) {
                    std::vector<float> result(ctx.buckets.size());
                    const float totalDuration = float(ctx.buckets.size());
                    float running = 0.0F;
                    for (size_t i = 0; i < ctx.buckets.size(); ++i) {
                        running += ctx.buckets[i].score;
                        const float elapsed = float(i) + 1.0F;
                        result[i] = running / elapsed * totalDuration;
                    }
                    return result;
                }
            },
            {
                // Same as ExpectedFinalScore but paced off trailing 5 seconds (reacts to hot/cold streaks)
                ColumnId::ExpectedFinalScoreRecent,
                [](const BuildContext &ctx) {
                    constexpr size_t kWindowSeconds = 5;
                    std::vector<float> result(ctx.buckets.size());
                    const float totalDuration = float(ctx.buckets.size());
                    for (size_t i = 0; i < ctx.buckets.size(); ++i) {
                        const size_t windowStart = i + 1 >= kWindowSeconds ? i + 1 - kWindowSeconds : 0;
                        float windowScore = 0.0F;
                        for (size_t j = windowStart; j <= i; ++j) windowScore += ctx.buckets[j].score;
                        const float windowLength = float(i - windowStart + 1);
                        const float pace = windowScore / windowLength;
                        result[i] = pace * totalDuration;
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

        // t=0 is always a phantom bucket (no data can land before the run starts); drop it
        // so the series doesn't open on a spurious zero point.
        buckets.erase(buckets.begin());

        result.times.resize(buckets.size());
        for (size_t s = 0; s < buckets.size(); ++s) result.times[s] = float(s + 1);

        const BuildContext context{perf, buckets};
        for (const auto &def: kColumnDefinitions) result.columns[def.id] = def.derive(context);

        return result;
    }
}
