//
// Created by Lecka on 02/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H
#define KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H
#include <string>

namespace ksv::application {
    class ISettingsService {
        public:
        virtual ~ISettingsService() = default;
        [[nodiscard]] virtual std::string getKovaaksDir() const = 0;
        virtual void setKovaaksDir(const std::string &dir) = 0;

        // Directory the profile cache (profile_cache.pb) is read from/written to.
        [[nodiscard]] virtual std::string getProfileDir() const = 0;
        virtual void setProfileDir(const std::string &dir) = 0;
    };
}
#endif //KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H
