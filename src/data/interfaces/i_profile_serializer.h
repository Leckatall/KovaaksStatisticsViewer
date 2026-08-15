//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PROFILE_SERIALIZER_H
#define KOVAAKSSTATSVIEWER_I_PROFILE_SERIALIZER_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>

#include "domain/user_profile.h"

namespace ksv::application {
    struct ProfileStoreHeader {
        std::uint32_t version = 0;
        std::int64_t created_at = 0;
        std::string name;
    };

    enum class ProfileLoadError {
        NotFound,
        Unparseable,
        VersionMismatch,
    };

    using ProfileLoadResult = std::variant<domain::UserProfile, ProfileLoadError>;

    class IProfileSerializer {
    public:
        virtual ~IProfileSerializer() = default;

        [[nodiscard]] virtual bool save(const domain::UserProfile& profile, const std::filesystem::path& path) = 0;

        [[nodiscard]] virtual std::optional<ProfileStoreHeader> readHeader(const std::filesystem::path& path) const = 0;

        [[nodiscard]] virtual ProfileLoadResult load(const std::filesystem::path& path) = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_PROFILE_SERIALIZER_H
