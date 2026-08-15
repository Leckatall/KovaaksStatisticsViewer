//
// Created by Lecka on 01/08/2026.
//

#include "profile_service.h"

#include "profile_builder.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <set>
#include <utility>

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
        m_file_service->onFilesChanged([this](const application::PerfFile &file) {
            addPerfFileToProfile(file);
        });
        m_settings_service->onProfilePathChanged([this] {
            applyProfilePath();
        });
    }

    void ProfileService::setProfile(domain::UserProfile profile) {
        m_profile = std::make_unique<domain::UserProfile>(std::move(profile));
        notifyProfileChanged();
    }

    void ProfileService::generateProfileFromDirectory() {
        setProfile(ProfileBuilder{m_file_service}.build());
        saveProfile();
    }

    void ProfileService::loadProfile() {
        const auto result = m_serializer->load(m_filepath);
        if (const auto stored = std::get_if<domain::UserProfile>(&result)) {
            setProfile(std::move(*stored));
            return;
        }
        if (m_build_requester) {
            m_build_requester();
            return;
        }
        generateProfileFromDirectory();
    }

    void ProfileService::beginProfileBuild() {
        m_build_in_flight = true;
    }

    void ProfileService::applyBuiltProfile(domain::UserProfile profile) {
        m_build_in_flight = false;

        // A settings change can replace the configured roots while a build is running.
        // The queued files survive a stale result so the follow-up build can replay them.
        std::set<std::string> profile_roots;
        for (const auto &source : profile.sources().entries()) {
            if (source.parent.value == 0) profile_roots.insert(source.path);
        }
        const auto current_roots_list = m_file_service->sourceRoots();
        const std::set current_roots(current_roots_list.begin(), current_roots_list.end());
        if (profile_roots != current_roots) return;

        const auto pending = std::exchange(m_pending_perf_files, {});
        setProfile(std::move(profile));

        bool replayed = false;
        for (const auto &perf_file: pending) {
            auto perf = m_file_service->getPerfFromFile(perf_file.absolutePath());
            perf.source = {m_profile->ensureSource(perf_file.root, perf_file.subdir), perf_file.filename};
            if (m_profile->getRun(perf.run_id)) continue;
            replayed |= m_profile->addScenarioPerf(perf);
        }
        if (replayed) notifyProfileChanged();
        saveProfile();
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

    void ProfileService::addPerfFileToProfile(const application::PerfFile &perf_file) {
        if (m_build_in_flight) {
            m_pending_perf_files.push_back(perf_file);
            return;
        }
        if (!m_profile) return;
        auto perf = m_file_service->getPerfFromFile(perf_file.absolutePath());
        perf.source = {m_profile->ensureSource(perf_file.root, perf_file.subdir), perf_file.filename};
        m_profile->addScenarioPerf(perf);
        notifyProfileChanged();
        saveProfile();
    }

    void ProfileService::saveProfile() const {
        if (!m_profile) return;
        if (!m_serializer->save(*m_profile, m_filepath)) {
            std::cerr << "Failed to save profile to " << m_filepath << std::endl;
        }
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

    std::vector<domain::RunData> ProfileService::getCompletionHistory(const domain::ScenarioId &scenario) const {
        if (!m_profile) return {};
        return m_profile->getCompletionHistory(scenario);
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
