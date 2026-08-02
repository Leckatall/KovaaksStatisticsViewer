//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_PROFILE_SERVICE_H
#define KOVAAKSSTATSVIEWER_PROFILE_SERVICE_H


#include <filesystem>
#include <memory>

#include "user_profile.h"
#include "../qt_data/interfaces/i_file_service.h"
#include "interfaces/i_profile_service.h"

namespace ksv::data {
    class ProfileService: public application::IProfileService{
    public:
        explicit ProfileService(std::shared_ptr<application::IFileService> file_service);
        void generateProfileFromDirectory() override;
        [[nodiscard]] std::vector<domain::ScenarioId> getScenarioList() const override;
        // TODO: implement profile persistence
        // void save_profile();
        // void load_profile();
        // void set_filepath(const QUrl& filepath);

    private:
        // QUrl m_filepath;
        std::filesystem::path m_filepath;
        std::unique_ptr<domain::UserProfile> m_profile;
        std::shared_ptr<application::IFileService> m_file_service;
    };
}


#endif //KOVAAKSSTATSVIEWER_PROFILE_SERVICE_H
