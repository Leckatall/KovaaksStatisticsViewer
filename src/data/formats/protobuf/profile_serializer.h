//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PROFILE_SERIALIZER_H
#define KOVAAKSSTATSVIEWER_PROFILE_SERIALIZER_H

#include <memory>

#include "formats/protobuf/schema/profile.pb.h"
#include "interfaces/i_profile_migrator.h"
#include "interfaces/i_profile_serializer.h"

namespace ksv::data {
    class ProfileSerializer: public application::IProfileSerializer {
    public:
        explicit ProfileSerializer(std::shared_ptr<IProfileMigrator> migrator = nullptr);

        [[nodiscard]] bool save(const domain::UserProfile& profile, const std::filesystem::path& path) override;
        [[nodiscard]] std::optional<application::ProfileStoreHeader> readHeader(
            const std::filesystem::path& path) const override;
        application::ProfileLoadResult load(const std::filesystem::path& path) override;

    private:
        std::shared_ptr<IProfileMigrator> m_migrator;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROFILE_SERIALIZER_H
