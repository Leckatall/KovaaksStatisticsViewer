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
        void save(const domain::UserProfile& profile, const std::filesystem::path& path) override;
        std::optional<domain::UserProfile> load(const std::filesystem::path& path) override;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROFILE_SERIALIZER_H
