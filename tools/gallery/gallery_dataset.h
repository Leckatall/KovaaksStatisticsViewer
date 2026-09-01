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
}
