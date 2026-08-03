//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_PROFILE_SERIALIZER_H
#define KOVAAKSSTATSVIEWER_I_PROFILE_SERIALIZER_H

#include <filesystem>
#include <optional>

#include "domain/user_profile.h"

namespace ksv::application {
    class IProfileSerializer {
    public:
        virtual ~IProfileSerializer() = default;

        virtual void save(const domain::UserProfile& profile, const std::filesystem::path& path) = 0;

        [[nodiscard]] virtual std::optional<domain::UserProfile> load(const std::filesystem::path& path) = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_PROFILE_SERIALIZER_H
