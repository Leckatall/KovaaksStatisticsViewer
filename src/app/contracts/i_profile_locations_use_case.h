#pragma once

#include <functional>
#include <string>

namespace ksv::application {
    // Where the profile is built from (the KovaaK's directory) and where it is saved, with the state of each.
    class IProfileLocationsUseCase {
    public:
        virtual ~IProfileLocationsUseCase() = default;

        // The first configured source root; empty when none is configured.
        [[nodiscard]] virtual std::string kovaaksDir() const = 0;
        // Replaces the first source root and keeps any others.
        virtual void setKovaaksDir(const std::string &dir) = 0;
        [[nodiscard]] virtual bool isKovaaksDirValid() const = 0;
        // Fires when the configured directory changes, and when its validity flips as files arrive under it.
        virtual void onKovaaksDirChanged(std::function<void()> callback) = 0;

        [[nodiscard]] virtual std::string profilePath() const = 0;
        virtual void setProfilePath(const std::string &path) = 0;
        virtual void onProfilePathChanged(std::function<void()> callback) = 0;

        [[nodiscard]] virtual bool isProfileLoaded() const = 0;
        virtual void onProfileChanged(std::function<void()> callback) = 0;
    };
}
