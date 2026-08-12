//
// Created by Lecka on 02/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H
#define KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H
#include <functional>
#include <string>

namespace ksv::application {
    class ISettingsService {
        public:
        virtual ~ISettingsService() = default;
        [[nodiscard]] virtual std::string getKovaaksDir() const = 0;
        [[nodiscard]] virtual bool isKovaaksDirSet() const = 0;
        virtual void setKovaaksDir(const std::string &dir) = 0;

        [[nodiscard]] virtual std::string getProfilePath() const = 0;
        virtual void setProfilePath(const std::string &path) = 0;

        virtual void onProfilePathChanged(std::function<void()> callback) = 0;
        virtual void onKovaaksDirChanged(std::function<void()> callback) = 0;
    };
}
#endif //KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H

