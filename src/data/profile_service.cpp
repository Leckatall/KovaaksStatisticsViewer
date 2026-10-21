//
// Created by Lecka on 01/08/2026.
//

#include "profile_service.h"

#include "profile_importer.h"
#include "source_path_key.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <numeric>
#include <set>
#include <utility>
#include <unordered_map>

namespace ksv::data {
    ProfileService::ProfileService(std::shared_ptr<application::IFileService> file_service,
                                   std::shared_ptr<application::IProfileSerializer> serializer,
                                   std::shared_ptr<application::ISettingsService> settings_service,
                                   std::shared_ptr<IRunIngestor> ingestor,
                                   std::shared_ptr<IProfileImporter> importer)
        : m_file_service(std::move(file_service)),
          m_serializer(std::move(serializer)),
          m_settings_service(std::move(settings_service)),
          m_ingestor(std::move(ingestor)) {
        m_profile = nullptr;
        m_importer = importer ? std::move(importer) : std::make_shared<ProfileImporter>(m_file_service, m_ingestor);
        m_filepath = m_settings_service->getProfilePath();
        ensureParentDir();
        const auto lifetime = std::weak_ptr(m_lifetime);
        m_file_service->onSourcesChanged([this, lifetime] {
            if (!lifetime.expired() && (m_refresh_requester || m_profile)) requestRefresh(application::ProfileImportReason::Live);
        });
        m_settings_service->onProfilePathChanged([this, lifetime] {
            if (!lifetime.expired()) applyProfilePath();
        });
        m_settings_service->onKovaaksDirsChanged([this, lifetime] {
            if (lifetime.expired()) return;
            invalidateImports();
            if (m_refresh_requester) requestRefresh(application::ProfileImportReason::SourceSettingsChanged);
        });
    }

    void ProfileService::setProfile(domain::UserProfile profile) {
        invalidateImports();
        m_profile = std::make_shared<domain::UserProfile>(std::move(profile));
        notifyProfileChanged();
    }

    void ProfileService::generateProfileFromDirectory() {
        auto result = m_importer->run(beginImport(application::ProfileImportMode::Rebuild,
            application::ProfileImportReason::Manual), {});
        if (acceptImport(result) == application::ProfileImportOutcome::Failed && result.summary.save_failed)
            std::cerr << "Failed to save profile to " << m_filepath << std::endl;
    }

    void ProfileService::loadProfile() {
        invalidateImports();
        const auto result = m_serializer->load(m_filepath);
        if (const auto stored = std::get_if<domain::UserProfile>(&result)) {
            setProfile(std::move(*stored));
            if (m_refresh_requester) requestRefresh(application::ProfileImportReason::Startup);
            return;
        }
        if (m_build_requester) {
            m_build_requester();
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

    void ProfileService::invalidateImports() {
        ++m_generation;
        m_active_import.reset();
        m_aliases.clear();
    }

    application::ProfileImportRequest ProfileService::beginImport(application::ProfileImportMode mode,
            application::ProfileImportReason reason) {
        application::ProfileImportRequest request;
        request.token = {m_generation, ++m_next_request};
        m_active_import = request.token;
        request.mode = mode;
        request.reason = reason;
        request.roots = m_file_service->sourceRoots();
        for (auto &root : request.roots) {
            std::error_code error;
            const auto absolute = std::filesystem::absolute(root, error);
            if (!error) root = absolute.lexically_normal().generic_string();
        }
        request.baseline = m_profile;
        request.known_aliases = m_aliases;
        using enum application::ProfileImportReason;
        request.skip_unchanged_failures = mode == application::ProfileImportMode::Refresh &&
            (reason == Startup || reason == Live || reason == SourceSettingsChanged);
        return request;
    }

    void ProfileService::acceptAliases(const application::ProfileImportResult &result) {
        std::unordered_map<domain::ScenarioRunId, domain::ScenarioRunId> rekeys;
        for (const auto &[old_id, new_id] : result.rekeyed_runs) rekeys.emplace(old_id, new_id);
        std::unordered_map<std::string, std::size_t> by_path;
        by_path.reserve(m_aliases.size() + result.successful_aliases.size());
        for (std::size_t index = 0; index < m_aliases.size(); ++index) {
            auto &alias = m_aliases[index];
            if (const auto found = rekeys.find(alias.run_id); found != rekeys.end()) alias.run_id = found->second;
            by_path.emplace(sourcePathKey(alias.path), index);
        }
        for (const auto &alias : result.successful_aliases) {
            auto [found, added] = by_path.emplace(sourcePathKey(alias.path), m_aliases.size());
            if (added) m_aliases.push_back(alias);
            else m_aliases[found->second] = alias;
        }
    }

    application::ProfileImportOutcome ProfileService::acceptImport(application::ProfileImportResult &result) {
        using application::ProfileImportOutcome;
        if (!m_active_import || *m_active_import != result.token || result.token.generation != m_generation)
            return ProfileImportOutcome::Stale;
        m_active_import.reset();
        if (result.failed) return ProfileImportOutcome::Failed;
        if (result.changed || result.failures_changed) {
            try {
                if (!result.candidate || !m_serializer->save(*result.candidate, m_filepath)) {
                    result.summary.save_failed = true;
                    return ProfileImportOutcome::Failed;
                }
            } catch (const std::exception &error) {
                result.summary.save_failed = true;
                result.summary.issues.push_back({m_filepath.string(), error.what(), false});
                return ProfileImportOutcome::Failed;
            }
            m_profile = result.candidate;
        }
        acceptAliases(result);
        std::vector<std::string> successful;
        std::vector<std::string> retryable;
        for (const auto &alias : result.successful_aliases) successful.push_back(alias.path);
        for (const auto &issue : result.summary.issues) if (issue.retryable) retryable.push_back(issue.path);
        m_file_service->acknowledgeImport(successful, retryable);
        if (result.changed) notifyProfileChanged();
        return result.summary.issues.empty() ? ProfileImportOutcome::Succeeded : ProfileImportOutcome::Partial;
    }

    void ProfileService::abandonImport(application::ProfileImportToken token) {
        if (m_active_import == token) m_active_import.reset();
    }

    void ProfileService::requestRefresh(application::ProfileImportReason reason) {
        if (m_refresh_requester) m_refresh_requester(reason);
        else {
            auto result = m_importer->run(beginImport(application::ProfileImportMode::Refresh, reason), {});
            acceptImport(result);
        }
    }

    std::vector<domain::ScenarioId> ProfileService::getScenarioList() const {
        if (m_profile == nullptr) {
            std::cerr << "Profile not loaded" << std::endl;
            return {};
        }
        return m_profile->getScenarioList();
    }

    domain::Run ProfileService::getPerf(const std::string &path) const {
        return m_file_service->getPerfFromFile(path);
    }

    domain::Run ProfileService::getLatestRun() const {
        if (!m_profile) return {};
        return m_profile->getLatestRun().value_or(domain::Run{});
    }

    std::optional<domain::Run> ProfileService::getMostRecentRun(const domain::ScenarioId &scenario) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getMostRecentRun(scenario);
    }

    std::vector<domain::Run> ProfileService::getMostRecentRuns(const domain::ScenarioId &scenario,
                                                                         const std::size_t count) const {
        if (!m_profile) return {};
        return m_profile->getMostRecentRuns(scenario, count);
    }

    std::vector<domain::Run> ProfileService::getRunsForScenario(const domain::ScenarioId &scenario) const {
        if (!m_profile) return {};
        return m_profile->getRunsForScenario(scenario);
    }

    std::vector<domain::RunSummary> ProfileService::getCompletionHistory(const domain::ScenarioId &scenario) const {
        if (!m_profile) return {};
        return m_profile->getCompletionHistory(scenario);
    }

    std::optional<float> ProfileService::getAverageScore(const domain::ScenarioId &scenario,
                                                         const std::size_t count) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getAverageScore(scenario, count);
    }

    std::optional<domain::Run> ProfileService::getCurrentRun(const domain::ScenarioRunId &run_id) const {
        if (!m_profile) return std::nullopt;
        return m_profile->getCurrentRun(run_id);
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

    std::vector<domain::Run> ProfileService::getRecentRuns(const std::size_t count) const {
        if (!m_profile || count == 0) return {};
        const auto &runs = m_profile->getAllRunRecords();

        std::vector<std::size_t> indices(runs.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::ranges::sort(indices, std::greater{}, [&runs](const std::size_t idx) {
            return runs[idx].run_id.start_time;
        });

        const auto n = std::min(count, indices.size());
        std::vector<domain::Run> result;
        result.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            result.push_back(runs[indices[i]]);
        }
        return result;
    }

    std::vector<std::pair<std::chrono::sys_days, double> >
    ProfileService::getRollingTimeAverage(const int window_days) const {
        if (!m_profile) return {};
        return m_profile->getRollingTimeAverage(window_days);
    }

    std::vector<domain::RunFact>
    ProfileService::getRunFacts(const std::vector<domain::ScenarioId> &scenarios) const {
        if (!m_profile) return {};
        return m_profile->getRunFacts(scenarios);
    }

    std::vector<std::pair<std::chrono::sys_days, double> >
    ProfileService::getRollingTimeAverage(const std::vector<domain::ScenarioId> &scenarios,
                                          const int window_days) const {
        if (!m_profile) return {};
        return m_profile->getRollingTimeAverage(scenarios, window_days);
    }
}
