#include "gallery_dataset.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ksv::gallery {
    namespace {
        using namespace std::chrono;

        const domain::ScenarioId kPrimary{
            .name = "1wall6targets Gallery",
            .hash = "gallery-primary",
        };
        const std::array kSecondary{
            domain::ScenarioId{.name = "Pasu Gallery", .hash = "gallery-pasu"},
            domain::ScenarioId{.name = "Smoothbot Gallery", .hash = "gallery-smoothbot"},
            domain::ScenarioId{.name = "VT FlyTS Gallery", .hash = "gallery-flyts"},
        };

        struct RunShape {
            int base_shots_per_second;
            int shot_variation;
            float base_accuracy;
            int hits_per_kill;
            float damage_per_hit;
            float score_per_hit;
            float score_per_kill;
        };

        RunShape shapeFor(const domain::ScenarioId &scenario) {
            if (scenario.hash == "gallery-pasu") return {4, 4, 0.68F, 1, 100.0F, 12.0F, 8.0F};
            if (scenario.hash == "gallery-smoothbot") return {8, 3, 0.86F, 18, 12.0F, 3.0F, 20.0F};
            if (scenario.hash == "gallery-flyts") return {5, 4, 0.74F, 3, 35.0F, 8.0F, 12.0F};
            return {6, 4, 0.78F, 1, 100.0F, 10.0F, 5.0F};
        }

        long long timestampMs(const int day_offset, const int minute_of_day) {
            const auto instant = sys_days{year{2026} / August / 1} + days{day_offset} + minutes{minute_of_day};
            return duration_cast<milliseconds>(instant.time_since_epoch()).count();
        }

        domain::Run makeRun(const domain::ScenarioId &scenario, const int day_offset, const int minute_of_day,
                            const float duration_seconds, const int seed) {
            domain::Run run{
                .run_id = {
                    .scenario_id = scenario,
                    .start_time = timestampMs(day_offset, minute_of_day),
                },
                .scenario_length = duration_seconds,
                .performance = domain::Performance{},
            };

            const auto shape = shapeFor(scenario);
            const int sample_count = static_cast<int>(std::round(duration_seconds));
            const float run_accuracy_adjustment = static_cast<float>(seed % 17 - 8) * 0.006F;
            int hits_since_kill = seed % shape.hits_per_kill;
            for (int sample = 0; sample < sample_count; ++sample) {
                const float time = static_cast<float>(sample) + 0.5F;
                const int shots = shape.base_shots_per_second
                                  + (sample * 5 + seed * 3) % shape.shot_variation;
                const float sample_accuracy = std::clamp(
                    shape.base_accuracy + run_accuracy_adjustment
                    + 0.04F * std::sin(static_cast<float>(sample * 3 + seed) * 0.31F),
                    0.5F, 0.95F);
                const int hits = std::clamp(static_cast<int>(std::round(shots * sample_accuracy)), 0, shots);
                const int misses = shots - hits;
                const int kills = (hits_since_kill + hits) / shape.hits_per_kill;
                hits_since_kill = (hits_since_kill + hits) % shape.hits_per_kill;
                const float damage = static_cast<float>(hits) * shape.damage_per_hit;
                const float possible_damage = static_cast<float>(shots) * shape.damage_per_hit;
                const float score = static_cast<float>(hits) * shape.score_per_hit
                                    + static_cast<float>(kills) * shape.score_per_kill;

                run.performance->add_data(time, domain::SHOTS, shots);
                run.performance->add_data(time, domain::HITS, hits);
                run.performance->add_data(time, domain::MISSES, misses);
                run.performance->add_data(time, domain::KILLS, kills);
                run.performance->add_data(time, domain::SCORE, score);
                run.performance->add_data(time, domain::DMG, damage);
                run.performance->add_data(time, domain::DMG_POSSIBLE, possible_damage);

                run.stored_totals.score += score;
                run.stored_totals.shots += shots;
                run.stored_totals.hits += hits;
                run.stored_totals.misses += misses;
                run.stored_totals.kills += kills;
            }
            return run;
        }

        domain::UserProfile richProfile() {
            domain::UserProfile profile;
            int seed = 0;
            for (const auto &scenario: kSecondary) {
                for (int run = 0; run < 8; ++run) {
                    const float duration = scenario.hash == "gallery-pasu" ? 45.0F : 60.0F;
                    profile.addRun(makeRun(scenario, run * 2, 10 * 60 + run * 7,
                                           duration, seed));
                    ++seed;
                }
            }
            for (int day = 0; day < 28; ++day) {
                for (int run = 0; run < 8; ++run) {
                    profile.addRun(makeRun(kPrimary, day, 18 * 60 + run * 2, 60.0F, seed++));
                }
            }
            return profile;
        }

        domain::UserProfile singleRunProfile() {
            domain::UserProfile profile;
            profile.addRun(makeRun(kPrimary, 27, 18 * 60, 60.0F, 17));
            return profile;
        }

        domain::UserProfile shortHistoryProfile() {
            domain::UserProfile profile;
            for (int run = 0; run < 4; ++run) {
                profile.addRun(makeRun(kPrimary, 24 + run, 18 * 60 + run * 5, 60.0F, 30 + run));
            }
            return profile;
        }

        domain::UserProfile bunchedHistoryProfile() {
            domain::UserProfile profile;
            for (int run = 0; run < 300; ++run) {
                profile.addRun(makeRun(kPrimary, 0, 8 * 60 + run, 60.0F, 50 + run));
            }
            for (int run = 0; run < 60; ++run) {
                profile.addRun(makeRun(kPrimary, 5, 18 * 60 + run, 60.0F, 350 + run));
            }
            return profile;
        }
    }

    domain::UserProfile makeProfile(const Dataset dataset) {
        switch (dataset) {
            case Dataset::RichProfile: return richProfile();
            case Dataset::SingleRun: return singleRunProfile();
            case Dataset::ShortHistory: return shortHistoryProfile();
            case Dataset::BunchedHistory: return bunchedHistoryProfile();
        }
        return domain::UserProfile{};
    }
}
