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

namespace ksv::data {
    class ProfileService: public application::IProfileService{
    public:
        explicit ProfileService(std::shared_ptr<application::IFileService> file_service,
                                 std::shared_ptr<application::IProfileSerializer> serializer,
                                 std::filesystem::path cache_path);

        // Path of the cache file ProfileService reads/writes for a given profile
        // directory. Callers should use this instead of hardcoding the filename.
        [[nodiscard]] static std::filesystem::path cachePathFor(const std::filesystem::path &dir);

        void generateProfileFromDirectory() override;
        void loadProfile() override;
        void addPerfFileToProfile(const std::string& perf_file) const;
        [[nodiscard]] std::vector<domain::ScenarioId> getScenarioList() const override;

        [[nodiscard]] domain::ScenarioPerf getPerf(const std::string& path) const override;
        [[nodiscard]] domain::ScenarioPerf getLatestPerf() const override;

        [[nodiscard]] std::optional<domain::ScenarioPerf> getMostRecentPerf(
            const domain::ScenarioId& scenario) const override;
        [[nodiscard]] std::optional<float> getAverageScore(
            const domain::ScenarioId& scenario, std::size_t count) const override;

        [[nodiscard]] bool isProfileLoaded() const override { return m_profile != nullptr; }
        void setProfileDirectory(const std::string &dir) override;

        void onProfileChanged(std::function<void()> callback) override {
            m_callbacks.push_back(std::move(callback));
        }

    private:
        static constexpr const char* kCacheFilename = "profile_cache.pb";

        void notifyProfileChanged() const {
            for (auto& cb : m_callbacks) cb();
        }
        void saveProfile() const;

        std::filesystem::path m_filepath;
        std::unique_ptr<domain::UserProfile> m_profile;
        std::shared_ptr<application::IFileService> m_file_service;
        std::shared_ptr<application::IProfileSerializer> m_serializer;

        std::vector<std::function<void()>> m_callbacks;
    };
}


#endif //KOVAAKSSTATSVIEWER_PROFILE_SERVICE_H
