//
// Created by Lecka on 01/08/2026.
//

#include "profile_service.h"

#include <algorithm>
#include <numeric>

namespace ksv::data {
    ProfileService::ProfileService(std::shared_ptr<application::IFileService> file_service,
                                   std::shared_ptr<application::IProfileSerializer> serializer,
                                   std::shared_ptr<application::ISettingsService> settings_service)
        : m_file_service(std::move(file_service)),
          m_serializer(std::move(serializer)),
          m_settings_service(std::move(settings_service)) {
        m_profile = nullptr;
        m_filepath = m_settings_service->getProfilePath();
        ensureParentDir();
        m_file_service->onFilesChanged([this](const std::string &path) {
            addPerfFileToProfile(path);
        });
        m_settings_service->onProfilePathChanged([this] {
            applyProfilePath();
        });
    }

    void ProfileService::generateProfileFromDirectory() {
        const auto perfs = m_file_service->getAllPerfsFromFiles();
        domain::UserProfile profile{m_file_service->getSourceDirectory()};
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

    void ProfileService::ensureParentDir() const {
        if (const auto parent = m_filepath.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
    }

    void ProfileService::applyProfilePath() {
        m_filepath = m_settings_service->getProfilePath();
        ensureParentDir();
        loadProfile();
    }

    void ProfileService::addPerfFileToProfile(const std::string &perf_file) const {
        if (!m_profile) return;
        const auto perf = m_file_service->getPerfFromFile(perf_file);
        m_profile->addScenarioPerf(perf);
        notifyProfileChanged();
        saveProfile();
    }

    void ProfileService::saveProfile() const {
        if (m_profile) m_serializer->save(*m_profile, m_filepath);
    }

    std::vector<domain::ScenarioId> ProfileService::getScenarioList() const {
        if (m_profile == nullptr) {
            std::cerr << "Profile not loaded" << std::endl;
            return {};
        }
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

    std::vector<domain::ScenarioPerf> ProfileService::getMostRecentPerfs(const domain::ScenarioId &scenario,
                                                                          const std::size_t count) const {
        if (!m_profile) return {};
        return m_profile->getMostRecentPerfs(scenario, count);
    }

    std::optional<float> ProfileService::getAverageScore(const domain::ScenarioId &scenario,
                                                         const std::size_t count) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getAverageScore(scenario, count);
    }

    std::optional<domain::ScenarioPerf> ProfileService::getRun(const domain::ScenarioRunId &run_id) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getRun(run_id);
    }

    std::optional<std::size_t> ProfileService::getRunCount(const domain::ScenarioId &scenario) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getRunCount(scenario);
    }

    std::optional<std::chrono::sys_seconds> ProfileService::getLastRunTime(const domain::ScenarioId &scenario) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getLastRunTime(scenario);
    }

    std::optional<double> ProfileService::getTotalTime(const domain::ScenarioId &scenario) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getTotalTime(scenario);
    }

    std::vector<domain::ScenarioPerf> ProfileService::getRecentRuns(const std::size_t count) const {
        if (!m_profile || count == 0) return {};
        const auto &runs = m_profile->getAllRunRecords();

        std::vector<std::size_t> indices(runs.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::ranges::sort(indices, std::greater{}, [&runs](const std::size_t idx) {
            return runs[idx].run_id.start_time;
        });

        const auto n = std::min(count, indices.size());
        std::vector<domain::ScenarioPerf> result;
        result.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            result.push_back(runs[indices[i]]);
        }
        return result;
    }

    std::vector<std::pair<std::chrono::sys_days, double>>
    ProfileService::getRollingTimeAverage(const int window_days) const {
        if (!m_profile) return {};
        return m_profile->getRollingTimeAverage(window_days);
    }
}
