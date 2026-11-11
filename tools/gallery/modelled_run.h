#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include <domain/run.h>

namespace ksv::gallery {
    // Per-second score = perHit*hits + perKill*kills + perMiss*misses + perDamage*damage.
    struct ScoringRules {
        double perHit = 1.0;
        double perKill = 0.0;
        double perMiss = 0.0;
        double perDamage = 0.0;
        int hitsPerKill = 1;
        double damagePerHit = 1.0;
    };

    // A scenario's rules and its ideal run; skill 1.0 scores what the ideal run scores.
    // The weights split a change in skill between accuracy and shot rate (in log space,
    // normalised to sum to 1): a constant-fire scenario has shotRateWeight 0.
    struct ScenarioModel {
        std::string name;
        std::string hash;
        double durationSeconds = 60.0;
        double firstSampleSeconds = 1.0;
        ScoringRules scoring;
        double idealShotsPerSecond = 1.0;
        double idealAccuracy = 1.0;
        double accuracyWeight = 1.0;
        double shotRateWeight = 0.0;
        std::optional<double> maxShotsPerSecond;
    };

    // Multiplies accuracy and shot rate over [from, to) seconds; a ramp moves
    // linearly from 1 at `from` to the multiplier at `to`.
    struct PerformanceWindow {
        double from = 0.0;
        double to = 0.0;
        double accuracy = 1.0;
        double shotRate = 1.0;
        bool ramp = false;
    };

    // The designed shape within a run. Noise is a per-second relative standard deviation.
    struct PerformanceShape {
        std::vector<PerformanceWindow> windows;
        double accuracyNoise = 0.0;
        double shotRateNoise = 0.0;
    };

    // splitmix64 with a hand-written Box-Muller: std's distributions differ between
    // standard libraries, and a spec must write the same bytes under MinGW and MSVC.
    class SpecRandom {
    public:
        explicit SpecRandom(std::uint64_t seed) : state_(seed) {}
        std::uint64_t next();
        double uniform();  // (0, 1)
        double normal();

    private:
        std::uint64_t state_;
    };

    [[nodiscard]] std::uint64_t mixSeed(std::initializer_list<std::uint64_t> parts);

    [[nodiscard]] double idealScore(const ScenarioModel &model);

    /**
     * @brief One run: the shaped, noisy ideal run, then scaled to `skill` x the ideal score.
     * @param scoreScale Multiplies every score (and so the ideal score) after fitting.
     */
    [[nodiscard]] domain::Run makeModelledRun(const domain::ScenarioId &scenario, int dayOffset, int minuteOfDay,
                                              const ScenarioModel &model, const PerformanceShape &shape, double skill,
                                              double scoreScale, std::uint64_t seed);
}
