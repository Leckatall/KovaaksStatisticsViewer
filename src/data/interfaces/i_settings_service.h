//
// Created by Lecka on 02/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H
#define KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H
#include <functional>
#include <string>
#include <vector>

namespace ksv::application {
    class ISettingsService {
        public:
        virtual ~ISettingsService() = default;
        [[nodiscard]] virtual std::vector<std::string> getKovaaksDirs() const = 0;
        [[nodiscard]] virtual bool isKovaaksDirSet() const = 0;
        virtual void setKovaaksDirs(const std::vector<std::string> &dirs) = 0;

        [[nodiscard]] virtual std::string getProfilePath() const = 0;
        virtual void setProfilePath(const std::string &path) = 0;

        virtual void onProfilePathChanged(std::function<void()> callback) = 0;
        virtual void onKovaaksDirsChanged(std::function<void()> callback) = 0;
    };
}
#endif //KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H

