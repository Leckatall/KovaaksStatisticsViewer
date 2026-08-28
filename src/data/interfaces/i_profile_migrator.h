#ifndef KOVAAKSSTATSVIEWER_I_PROFILE_MIGRATOR_H
#define KOVAAKSSTATSVIEWER_I_PROFILE_MIGRATOR_H

#include <cstdint>
#include <filesystem>
#include <optional>

#include "domain/user_profile.h"

namespace ksv::data {
    class IProfileMigrator {
    public:
        virtual ~IProfileMigrator() = default;

        // nullopt when from_version is not a version this migrator can translate, or
        // when the on-disk file cannot be decoded as that version.
        [[nodiscard]] virtual std::optional<domain::UserProfile> migrate(
            const std::filesystem::path &path, std::uint32_t from_version) const = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_PROFILE_MIGRATOR_H
