#include "profile_locations_use_case.h"

#include <utility>

namespace ksv::application {
    ProfileLocationsUseCase::ProfileLocationsUseCase(std::shared_ptr<ISettingsService> settings,
                                                     std::shared_ptr<IFileService> files,
                                                     std::shared_ptr<IProfileService> profile)
        : m_settings(std::move(settings)), m_files(std::move(files)), m_profile(std::move(profile)),
          m_kovaaksDirValid(m_files->hasValidSourceRoots()) {
        const auto lifetime = std::weak_ptr(m_lifetime);
        m_settings->onKovaaksDirsChanged([this, lifetime] {
            if (!lifetime.expired()) kovaaksDirChanged();
        });
        m_files->onSourcesChanged([this, lifetime] {
            if (!lifetime.expired()) sourcesChanged();
        });
        m_settings->onProfilePathChanged([this, lifetime] {
            if (lifetime.expired()) return;
            for (const auto &callback: m_profilePathCallbacks) callback();
        });
        m_profile->onProfileChanged([this, lifetime] {
            if (lifetime.expired()) return;
            for (const auto &callback: m_profileCallbacks) callback();
        });
    }

    std::string ProfileLocationsUseCase::kovaaksDir() const {
        const auto dirs = m_settings->getKovaaksDirs();
        return dirs.empty() ? std::string() : dirs.front();
    }

    void ProfileLocationsUseCase::setKovaaksDir(const std::string &dir) {
        auto dirs = m_settings->getKovaaksDirs();
        if (!dirs.empty() && dirs.front() == dir) return;
        if (dirs.empty()) dirs.push_back(dir);
        else dirs.front() = dir;
        m_settings->setKovaaksDirs(dirs);
    }

    void ProfileLocationsUseCase::onKovaaksDirChanged(std::function<void()> callback) {
        m_kovaaksDirCallbacks.push_back(std::move(callback));
    }

    void ProfileLocationsUseCase::setProfilePath(const std::string &path) {
        if (path == m_settings->getProfilePath()) return;
        m_settings->setProfilePath(path);
    }

    void ProfileLocationsUseCase::onProfilePathChanged(std::function<void()> callback) {
        m_profilePathCallbacks.push_back(std::move(callback));
    }

    void ProfileLocationsUseCase::onProfileChanged(std::function<void()> callback) {
        m_profileCallbacks.push_back(std::move(callback));
    }

    void ProfileLocationsUseCase::kovaaksDirChanged() {
        m_kovaaksDirValid = m_files->hasValidSourceRoots();
        for (const auto &callback: m_kovaaksDirCallbacks) callback();
    }

    void ProfileLocationsUseCase::sourcesChanged() {
        const bool valid = m_files->hasValidSourceRoots();
        if (valid == m_kovaaksDirValid) return;
        m_kovaaksDirValid = valid;
        for (const auto &callback: m_kovaaksDirCallbacks) callback();
    }
}
