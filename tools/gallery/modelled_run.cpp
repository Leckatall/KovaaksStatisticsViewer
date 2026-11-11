#include "modelled_run.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

namespace ksv::gallery {
    namespace {
        long long timestampMs(const int dayOffset, const int minuteOfDay) {
            using namespace std::chrono;
            const auto instant = sys_days{year{2026} / August / 1} + days{dayOffset} + minutes{minuteOfDay};
            return duration_cast<milliseconds>(instant.time_since_epoch()).count();
        }

        struct Interval {
            double start;
            double end;
            [[nodiscard]] double length() const { return end - start; }
            [[nodiscard]] double middle() const { return (start + end) / 2.0; }
        };

        std::vector<Interval> intervals(const ScenarioModel &model) {
            std::vector<Interval> result;
            const double duration = model.durationSeconds;
            double start = 0.0;
            double end = std::min(model.firstSampleSeconds, duration);
            while (end - start > 1e-9) {
                result.push_back({start, end});
                start = end;
                end = std::min(end + 1.0, duration);
            }
            return result;
        }

        // Shots and accuracy of one interval before the level is fitted.
        struct Behaviour {
            double shots;
            double accuracy;
        };

        double windowMultiplier(const PerformanceWindow &window, const double value, const double time) {
            if (time < window.from || time >= window.to) return 1.0;
            if (!window.ramp) return value;
            return 1.0 + (value - 1.0) * (time - window.from) / (window.to - window.from);
        }

        double continuousScore(const ScoringRules &rules, const double shots, const double accuracy) {
            const double hits = shots * accuracy;
            return rules.perHit * hits + rules.perKill * hits / rules.hitsPerKill + rules.perMiss * (shots - hits) +
                   rules.perDamage * hits * rules.damagePerHit;
        }

        struct Level {
            double shotFactor;
            double accuracyFactor;
        };

        Level levelAt(const ScenarioModel &model, const double logLevel) {
            const double total = model.accuracyWeight + model.shotRateWeight;
            double shotFactor = std::exp(logLevel * model.shotRateWeight / total);
            if (model.maxShotsPerSecond) {
                shotFactor = std::min(shotFactor, *model.maxShotsPerSecond / model.idealShotsPerSecond);
            }
            return {shotFactor, std::exp(logLevel * model.accuracyWeight / total)};
        }

        double totalScore(const ScenarioModel &model, const std::vector<Behaviour> &base, const Level level) {
            double score = 0.0;
            for (const auto &interval: base) {
                score += continuousScore(model.scoring, interval.shots * level.shotFactor,
                                         std::min(1.0, interval.accuracy * level.accuracyFactor));
            }
            return score;
        }

        // Bisects the level whose score meets the target. Fitting on expected
        // (unrounded) counts is close enough: targets are approximate by design.
        Level fitLevel(const ScenarioModel &model, const std::vector<Behaviour> &base, const double target) {
            double low = -12.0;
            double high = 12.0;
            if (target <= totalScore(model, base, levelAt(model, low))) return levelAt(model, low);
            if (target >= totalScore(model, base, levelAt(model, high))) return levelAt(model, high);
            for (int iteration = 0; iteration < 80; ++iteration) {
                const double middle = (low + high) / 2.0;
                (totalScore(model, base, levelAt(model, middle)) < target ? low : high) = middle;
            }
            return levelAt(model, (low + high) / 2.0);
        }
    }

    std::uint64_t SpecRandom::next() {
        state_ += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    double SpecRandom::uniform() {
        return (static_cast<double>(next() >> 11) + 0.5) / 9007199254740992.0;
    }

    double SpecRandom::normal() {
        const double radius = std::sqrt(-2.0 * std::log(uniform()));
        return radius * std::cos(2.0 * std::numbers::pi * uniform());
    }

    std::uint64_t mixSeed(const std::initializer_list<std::uint64_t> parts) {
        SpecRandom random(0x5EEDULL);
        std::uint64_t seed = random.next();
        for (const std::uint64_t part: parts) seed = SpecRandom(seed ^ part).next();
        return seed;
    }

    double idealScore(const ScenarioModel &model) {
        double score = 0.0;
        for (const auto &interval: intervals(model)) {
            score += continuousScore(model.scoring, model.idealShotsPerSecond * interval.length(), model.idealAccuracy);
        }
        return score;
    }

    domain::Run makeModelledRun(const domain::ScenarioId &scenario, const int dayOffset, const int minuteOfDay,
                                const ScenarioModel &model, const PerformanceShape &shape, const double skill,
                                const double scoreScale, const std::uint64_t seed) {
        domain::Run run{
            .run_id = {.scenario_id = scenario, .start_time = timestampMs(dayOffset, minuteOfDay)},
            .scenario_length = static_cast<float>(model.durationSeconds),
            .performance = domain::Performance{},
        };

        // Shape first: the ideal run with its windows and per-second noise...
        const std::vector<Interval> layout = intervals(model);
        std::vector<Behaviour> base;
        SpecRandom random(seed);
        for (const auto &interval: layout) {
            double shotMultiplier = 1.0;
            double accuracyMultiplier = 1.0;
            for (const auto &window: shape.windows) {
                shotMultiplier *= windowMultiplier(window, window.shotRate, interval.middle());
                accuracyMultiplier *= windowMultiplier(window, window.accuracy, interval.middle());
            }
            const double shotNoise = std::max(0.0, 1.0 + shape.shotRateNoise * random.normal());
            const double accuracyNoise = std::max(0.0, 1.0 + shape.accuracyNoise * random.normal());
            base.push_back({model.idealShotsPerSecond * interval.length() * shotMultiplier * shotNoise,
                            model.idealAccuracy * accuracyMultiplier * accuracyNoise});
        }

        // ...then scaled to the skill's target, so a shape moves score around a run
        // without raising or lowering it.
        const Level level = fitLevel(model, base, std::max(0.0, skill) * idealScore(model));

        const ScoringRules &rules = model.scoring;
        double shotCarry = 0.0;
        double hitCarry = 0.0;
        int hitsTowardKill = 0;
        float totalDamage = 0.0F;
        for (std::size_t i = 0; i < layout.size(); ++i) {
            shotCarry += base[i].shots * level.shotFactor;
            const int shots = std::max(0, static_cast<int>(std::floor(shotCarry + 0.5)));
            shotCarry -= shots;
            hitCarry += shots * std::min(1.0, base[i].accuracy * level.accuracyFactor);
            const int hits = std::clamp(static_cast<int>(std::floor(hitCarry + 0.5)), 0, shots);
            hitCarry -= hits;
            const int misses = shots - hits;
            const int kills = (hitsTowardKill + hits) / rules.hitsPerKill;
            hitsTowardKill = (hitsTowardKill + hits) % rules.hitsPerKill;
            const double damage = hits * rules.damagePerHit;
            const double score = scoreScale * (rules.perHit * hits + rules.perKill * kills + rules.perMiss * misses +
                                               rules.perDamage * damage);

            const auto time = static_cast<float>(layout[i].end);
            run.performance->add_data(time, domain::SHOTS, shots);
            run.performance->add_data(time, domain::HITS, hits);
            run.performance->add_data(time, domain::MISSES, misses);
            run.performance->add_data(time, domain::KILLS, kills);
            run.performance->add_data(time, domain::SCORE, static_cast<float>(score));
            run.performance->add_data(time, domain::DMG, static_cast<float>(damage));
            run.performance->add_data(time, domain::DMG_POSSIBLE, static_cast<float>(shots * rules.damagePerHit));

            run.stored_totals.score += static_cast<float>(score);
            run.stored_totals.shots += shots;
            run.stored_totals.hits += hits;
            run.stored_totals.misses += misses;
            run.stored_totals.kills += kills;
            totalDamage += static_cast<float>(damage);
        }
        run.stored_totals.damage = totalDamage;
        return run;
    }
}
