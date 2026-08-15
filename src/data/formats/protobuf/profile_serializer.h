//
// Created by Lecka on 03/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PROFILE_SERIALIZER_H
#define KOVAAKSSTATSVIEWER_PROFILE_SERIALIZER_H

#include "formats/protobuf/schema/store.pb.h"
#include "interfaces/i_profile_serializer.h"

namespace ksv::data {
    class ProfileSerializer: public application::IProfileSerializer {
    public:
        [[nodiscard]] bool save(const domain::UserProfile& profile, const std::filesystem::path& path) override;
        [[nodiscard]] std::optional<application::ProfileStoreHeader> readHeader(
            const std::filesystem::path& path) const override;
        application::ProfileLoadResult load(const std::filesystem::path& path) override;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROFILE_SERIALIZER_H
