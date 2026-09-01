#include "bucketed_run.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace ksv::application {
    namespace {
        constexpr int kMaximumBucketedSeconds = 3600;

        std::optional<int> bucketIndexFor(const float time) {
            if (!std::isfinite(time) || time < 0.0F || time > static_cast<float>(kMaximumBucketedSeconds)) return {};
            return static_cast<int>(std::lround(time));
        }
    }

    const std::vector<double> &BucketedRun::valuesFor(const PrimitiveMetric metric) const {
        switch (metric) {
            case PrimitiveMetric::Score: return score;
            case PrimitiveMetric::Shots: return shots;
            case PrimitiveMetric::Hits: return hits;
            case PrimitiveMetric::Kills: return kills;
            case PrimitiveMetric::Dmg: return dmg;
        }
        throw std::invalid_argument("Unknown primitive metric");
    }

    BucketedRun bucketRun(const domain::Run &run) {
        BucketedRun result;
        if (!run.performance) return result;
        const auto &samples = run.performance->samples;
        if (samples.empty()) return result;
        int maximum = 0;
        for (const auto &point: samples) {
            if (const auto index = bucketIndexFor(point.time)) maximum = std::max(maximum, *index);
        }
        const auto size = static_cast<size_t>(maximum + 1);
        result.score.resize(size);
        result.shots.resize(size);
        result.hits.resize(size);
        result.kills.resize(size);
        result.dmg.resize(size);
        for (const auto &point: samples) {
            const auto bucket = bucketIndexFor(point.time);
            if (!bucket) continue;
            const auto index = static_cast<size_t>(*bucket);
            result.score[index] += point.score;
            result.shots[index] += point.shots;
            result.hits[index] += point.hits;
            result.kills[index] += point.kills;
            result.dmg[index] += point.dmg;
        }
        // TODO: Why not just use std::clamp(lo: 1)?
        const auto drop_first = [](auto &values) { values.erase(values.begin()); };
        drop_first(result.score);
        drop_first(result.shots);
        drop_first(result.hits);
        drop_first(result.kills);
        drop_first(result.dmg);
        result.times.resize(static_cast<size_t>(maximum));
        for (size_t i = 0; i < result.times.size(); ++i) result.times[i] = static_cast<float>(i + 1);
        return result;
    }
}
