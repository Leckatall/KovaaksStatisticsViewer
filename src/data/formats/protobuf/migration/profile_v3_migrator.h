#ifndef KOVAAKSSTATSVIEWER_PROFILE_V3_MIGRATOR_H
#define KOVAAKSSTATSVIEWER_PROFILE_V3_MIGRATOR_H

#include <memory>

#include "interfaces/i_profile_migrator.h"
#include "interfaces/i_run_ingestor.h"

namespace ksv::data {
    // Only translation unit that pulls in the retired v3 protobuf descriptor
    // (profile_v3.pb.h). Kept out of ProfileSerializer so the current reader never
    // links a legacy schema.
    class ProfileV3Migrator final : public IProfileMigrator {
    public:
        explicit ProfileV3Migrator(std::shared_ptr<IRunIngestor> ingestor);

        [[nodiscard]] std::optional<domain::UserProfile> migrate(
            const std::filesystem::path &path, std::uint32_t from_version) const override;

    private:
        std::shared_ptr<IRunIngestor> m_ingestor;
    };
}

#endif //KOVAAKSSTATSVIEWER_PROFILE_V3_MIGRATOR_H
