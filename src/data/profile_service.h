//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PROFILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_PROFILE_SERVICE_H


#include <filesystem>
#include <memory>
#include <optional>

#include "user_profile.h"
#include "interfaces/i_file_service.h"
#include "interfaces/i_profile_service.h"
#include "interfaces/i_profile_serializer.h"
#include "interfaces/i_run_ingestor.h"
#include "interfaces/i_settings_service.h"
#include "interfaces/i_profile_importer.h"

namespace ksv::data {
    class ProfileService : public application::IProfileService {
    public:
        ProfileService(std::shared_ptr<application::IFileService> file_service,
                       std::shared_ptr<application::IProfileSerializer> serializer,
                       std::shared_ptr<application::ISettingsService> settings_service,
                       std::shared_ptr<IRunIngestor> ingestor,
                       std::shared_ptr<IProfileImporter> importer = {});

        [[nodiscard]] application::ProfileImportRequest beginImport(application::ProfileImportMode, application::ProfileImportReason) override;
        application::ProfileImportOutcome acceptImport(application::ProfileImportResult &) override;
        void abandonImport(application::ProfileImportToken) override;
        void onRefreshRequested(std::function<void(application::ProfileImportReason)> callback) override {
            m_refresh_requester = std::move(callback);
        }

        void generateProfileFromDirectory() override;

        void loadProfile() override;

        void setProfile(domain::UserProfile profile);

        void onBuildRequested(std::function<void()> callback) override {
            m_build_requester = std::move(callback);
        }

        [[nodiscard]] std::vector<domain::ScenarioId> getScenarioList() const override;

        [[nodiscard]] domain::Run getPerf(const std::string &path) const override;

        [[nodiscard]] domain::Run getLatestRun() const override;

        [[nodiscard]] std::optional<domain::Run> getMostRecentRun(
            const domain::ScenarioId &scenario) const override;

        [[nodiscard]] std::vector<domain::Run> getMostRecentRuns(
            const domain::ScenarioId &scenario, std::size_t count) const override;

        [[nodiscard]] std::vector<domain::Run> getRunsForScenario(
            const domain::ScenarioId &scenario) const override;

        [[nodiscard]] std::vector<domain::RunSummary>
        getCompletionHistory(const domain::ScenarioId &scenario) const override;

        [[nodiscard]] std::optional<float> getAverageScore(
            const domain::ScenarioId &scenario, std::size_t count) const override;

        [[nodiscard]] std::optional<domain::Run> getCurrentRun(
            const domain::ScenarioRunId &run_id) const override;

        [[nodiscard]] std::optional<std::size_t> getRunCount(
            const domain::ScenarioId &scenario) const override;

        [[nodiscard]] std::optional<std::chrono::sys_seconds> getLastRunTime(
            const domain::ScenarioId &scenario) const override;

        [[nodiscard]] std::optional<double> getTotalTime(
            const domain::ScenarioId &scenario) const override;

        [[nodiscard]] std::vector<domain::Run> getRecentRuns(std::size_t count) const override;

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(int window_days) const override;

        [[nodiscard]] std::vector<domain::RunFact> getRunFacts(
            const std::vector<domain::ScenarioId> &scenarios) const override;

        [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double> >
        getRollingTimeAverage(const std::vector<domain::ScenarioId> &scenarios, int window_days) const override;

        [[nodiscard]] bool isProfileLoaded() const override { return m_profile != nullptr; }

        void onProfileChanged(std::function<void()> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

    private:
        void notifyProfileChanged() const {
            for (auto &cb: m_callbacks) cb();
        }

        void ensureParentDir() const;

        void applyProfilePath();
        void invalidateImports();
        void requestRefresh(application::ProfileImportReason);
        void acceptAliases(const application::ProfileImportResult &);

        std::filesystem::path m_filepath;
        std::shared_ptr<const domain::UserProfile> m_profile;
        std::shared_ptr<IProfileImporter> m_importer;
        std::function<void(application::ProfileImportReason)> m_refresh_requester;
        std::uint64_t m_generation = 0;
        std::uint64_t m_next_request = 0;
        std::optional<application::ProfileImportToken> m_active_import;
        std::vector<application::ProfileSourceAlias> m_aliases;
        std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
        std::function<void()> m_build_requester;
        std::shared_ptr<application::IFileService> m_file_service;
        std::shared_ptr<application::IProfileSerializer> m_serializer;
        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<IRunIngestor> m_ingestor;

        std::vector<std::function<void()> > m_callbacks;
    };
}


#endif //KOVAAKSSTATSVIEWER_PROFILE_SERVICE_H
