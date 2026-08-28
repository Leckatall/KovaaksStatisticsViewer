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

namespace ksv::data {
    class ProfileService : public application::IProfileService {
    public:
        ProfileService(std::shared_ptr<application::IFileService> file_service,
                       std::shared_ptr<application::IProfileSerializer> serializer,
                       std::shared_ptr<application::ISettingsService> settings_service,
                       std::shared_ptr<IRunIngestor> ingestor);

        void generateProfileFromDirectory() override;

        void loadProfile() override;

        void addPerfFileToProfile(const application::PerfFile &perf_file);

        void setProfile(domain::UserProfile profile);

        void onBuildRequested(std::function<void()> callback) override {
            m_build_requester = std::move(callback);
        }

        void beginProfileBuild() override;

        void applyBuiltProfile(domain::UserProfile profile) override;

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

        [[nodiscard]] bool isProfileLoaded() const override { return m_profile != nullptr; }

        void onProfileChanged(std::function<void()> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

    private:
        void notifyProfileChanged() const {
            for (auto &cb: m_callbacks) cb();
        }

        void saveProfile() const;

        void ensureParentDir() const;

        void applyProfilePath();

        std::filesystem::path m_filepath;
        std::unique_ptr<domain::UserProfile> m_profile;
        std::function<void()> m_build_requester;
        bool m_build_in_flight = false;
        std::vector<application::PerfFile> m_pending_perf_files;
        std::shared_ptr<application::IFileService> m_file_service;
        std::shared_ptr<application::IProfileSerializer> m_serializer;
        std::shared_ptr<application::ISettingsService> m_settings_service;
        std::shared_ptr<IRunIngestor> m_ingestor;

        std::vector<std::function<void()> > m_callbacks;
    };
}


#endif //KOVAAKSSTATSVIEWER_PROFILE_SERVICE_H
