//
// Created by Lecka on 01/08/2026.
//

#include "profile_service.h"

namespace ksv::data {
    ProfileService::ProfileService(std::shared_ptr<application::IFileService> file_service,
                                   std::shared_ptr<application::IProfileSerializer> serializer,
                                   std::filesystem::path cache_path) : m_filepath(std::move(cache_path)),
        m_file_service(std::move(file_service)), m_serializer(std::move(serializer)) {
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
        saveProfile();
    }

    void ProfileService::loadProfile() {
        if (auto cached = m_serializer->load(m_filepath)) {
            m_profile = std::make_unique<domain::UserProfile>(std::move(*cached));
            notifyProfileChanged();
            return;
        }
        generateProfileFromDirectory();
    }

    std::filesystem::path ProfileService::cachePathFor(const std::filesystem::path &dir) {
        return dir / kCacheFilename;
    }

    void ProfileService::setProfileDirectory(const std::string &dir) {
        std::filesystem::create_directories(dir);
        m_filepath = cachePathFor(dir);
        loadProfile();
    }

    void ProfileService::addPerfFileToProfile(const std::string &perf_file) const {
        const auto perf = m_file_service->getPerfFromFile(perf_file);
        m_profile->addScenarioPerf(perf);
        notifyProfileChanged();
        saveProfile();
    }

    void ProfileService::saveProfile() const {
        if (m_profile) m_serializer->save(*m_profile, m_filepath);
    }

    std::vector<domain::ScenarioId> ProfileService::getScenarioList() const {
        if (m_profile == nullptr) {std::cerr << "Profile not loaded" << std::endl; return {};}
        return m_profile->getScenarioList();
    }

    domain::ScenarioPerf ProfileService::getPerf(const std::string &path) const {
        return m_file_service->getPerfFromFile(path);
    }

    domain::ScenarioPerf ProfileService::getLatestPerf() const {
        if (!m_profile) return {};
        return m_profile->getMostRecentPerf().value_or(domain::ScenarioPerf{});
    }

    std::optional<domain::ScenarioPerf> ProfileService::getMostRecentPerf(const domain::ScenarioId &scenario) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getMostRecentPerf(scenario);
    }

    std::optional<float> ProfileService::getAverageScore(const domain::ScenarioId &scenario,
                                                          const std::size_t count) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getAverageScore(scenario, count);
    }
}
