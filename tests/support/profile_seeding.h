#ifndef KOVAAKSSTATSVIEWER_TESTS_PROFILE_SEEDING_H
#define KOVAAKSSTATSVIEWER_TESTS_PROFILE_SEEDING_H

#include <memory>
#include <utility>

#include "data/interfaces/i_profile_service.h"

namespace ksv::tests_support {
    // Installs `profile` as the service's profile through the public import contract.
    inline application::ProfileImportOutcome seedProfile(application::IProfileService &service, domain::UserProfile profile) {
        const auto request = service.beginImport(application::ProfileImportMode::Rebuild,
                                                 application::ProfileImportReason::Manual);
        application::ProfileImportResult result;
        result.token = request.token;
        result.candidate = std::make_shared<const domain::UserProfile>(std::move(profile));
        result.changed = true;
        return service.acceptImport(result);
    }
}

#endif
