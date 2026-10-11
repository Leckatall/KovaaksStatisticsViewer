#pragma once

#include <array>
#include <string_view>

#include <domain/user_profile.h>

namespace ksv::gallery {
    enum class Dataset {
        RichProfile,
        SingleRun,
        ShortHistory,
        BunchedHistory,
    };

    inline constexpr std::array kDatasetNames{
        std::string_view{"Rich profile"},
        std::string_view{"Single run"},
        std::string_view{"Short history"},
        std::string_view{"Bunched history"},
    };
    [[nodiscard]] domain::UserProfile makeProfile(Dataset dataset);

    // How makeRun synthesizes a run's per-second samples.
    struct RunShape {
        int base_shots_per_second;
        int shot_variation;
        float base_accuracy;
        int hits_per_kill;
        float damage_per_hit;
        float score_per_hit;
        float score_per_kill;
    };

    // The shape the gallery uses for a scenario hash (gallery-pasu,
    // gallery-smoothbot, gallery-flyts; anything else gets the primary shape).
    [[nodiscard]] RunShape shapeForHash(std::string_view hash);

    // One deterministic run: `seed` varies accuracy and shot counts; timestamps
    // count from 2026-08-01 00:00 UTC.
    [[nodiscard]] domain::Run makeRun(const domain::ScenarioId &scenario, int day_offset, int minute_of_day,
                                      float duration_seconds, int seed, const RunShape &shape);
}
