//
// Created by Lecka on 01/08/2026.
//

#include "profile_service.h"

namespace ksv::data {
    ProfileService::ProfileService(std::shared_ptr<application::IFileService> file_service) : m_file_service(
        std::move(file_service)) {
        m_profile = nullptr;
    }

    void ProfileService::generateProfileFromDirectory() {
        const auto perfs = m_file_service->getAllPerfsFromFiles();
        domain::UserProfile profile{"default"};
        for (const auto &perf: perfs) {
            profile.addScenarioPerf(perf);
        }
        m_profile = std::make_unique<domain::UserProfile>(profile);
    }

    std::vector<domain::ScenarioId> ProfileService::getScenarioList() const {
        if (m_profile == nullptr) {std::cerr << "Profile not loaded" << std::endl; return {};}
        return m_profile->getScenarioList();
    }
}
