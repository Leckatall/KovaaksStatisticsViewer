#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "contracts/i_profile_locations_use_case.h"
#include "data/interfaces/i_file_service.h"
#include "data/interfaces/i_profile_service.h"
#include "data/interfaces/i_settings_service.h"

namespace ksv::application {
    class ProfileLocationsUseCase final : public IProfileLocationsUseCase {
    public:
        ProfileLocationsUseCase(std::shared_ptr<ISettingsService> settings, std::shared_ptr<IFileService> files,
                                std::shared_ptr<IProfileService> profile);

        [[nodiscard]] std::string kovaaksDir() const override;
        void setKovaaksDir(const std::string &dir) override;
        [[nodiscard]] bool isKovaaksDirValid() const override { return m_files->hasValidSourceRoots(); }
        void onKovaaksDirChanged(std::function<void()> callback) override;

        [[nodiscard]] std::string profilePath() const override { return m_settings->getProfilePath(); }
        void setProfilePath(const std::string &path) override;
        void onProfilePathChanged(std::function<void()> callback) override;

        [[nodiscard]] bool isProfileLoaded() const override { return m_profile->isProfileLoaded(); }
        void onProfileChanged(std::function<void()> callback) override;

    private:
        void kovaaksDirChanged();
        void sourcesChanged();

        std::shared_ptr<ISettingsService> m_settings;
        std::shared_ptr<IFileService> m_files;
        std::shared_ptr<IProfileService> m_profile;
        bool m_kovaaksDirValid;
        std::vector<std::function<void()> > m_kovaaksDirCallbacks;
        std::vector<std::function<void()> > m_profilePathCallbacks;
        std::vector<std::function<void()> > m_profileCallbacks;
        std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);
    };
}
