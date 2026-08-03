//
// Created by Lecka on 01/08/2026.
//

#include "profile_service.h"

namespace ksv::data {
    ProfileService::ProfileService(std::shared_ptr<application::IFileService> file_service) : m_file_service(
        std::move(file_service)) {
        m_profile = nullptr;
        m_file_service->onFilesChanged([this](const std::string& path) {
            addPerfFileToProfile(path);
        });
    }

    void ProfileService::generateProfileFromDirectory() {
        const auto perfs = m_file_service->getAllPerfsFromFiles();
        domain::UserProfile profile{"default"};
        for (const auto &perf: perfs) {
            profile.addScenarioPerf(perf);
        }
        m_profile = std::make_unique<domain::UserProfile>(profile);
        notifyProfileChanged();
    }

    void ProfileService::addPerfFileToProfile(const std::string &perf_file) const {
        const auto perf = m_file_service->getPerfFromFile(perf_file);
        m_profile->addScenarioPerf(perf);
        notifyProfileChanged();
    }

    std::vector<domain::ScenarioId> ProfileService::getScenarioList() const {
        if (m_profile == nullptr) {std::cerr << "Profile not loaded" << std::endl; return {};}
        return m_profile->getScenarioList();
    }

    domain::ScenarioPerf ProfileService::getPerf(const std::string &path) const {
        //TODO: Is allowing perfs to be loaded by file rather than through a scenarioRunId bad?
        return m_file_service->getPerfFromFile(path);
    }

    domain::ScenarioPerf ProfileService::getLatestPerf() const {
        //TODO: Move the get latest logic into the Profile.
        return m_file_service->getLatestPerf();
    }
}
