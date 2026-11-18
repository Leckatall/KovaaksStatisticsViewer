#ifndef KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_PROFILE_LOCATIONS_USE_CASE_H
#define KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_PROFILE_LOCATIONS_USE_CASE_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "app/contracts/i_profile_locations_use_case.h"

namespace ksv::tests_support {
    // Setters store the value and notify unconditionally; tests change fields directly and call the
    // notify functions to simulate a change made elsewhere.
    class FakeProfileLocationsUseCase final : public application::IProfileLocationsUseCase {
    public:
        std::string kovaaks_dir = "C:/Kovaaks";
        bool kovaaks_dir_valid = true;
        std::string profile_path = "C:/Profile/profile.pb";
        bool profile_loaded = false;
        std::vector<std::string> kovaaks_dir_writes;
        std::vector<std::string> profile_path_writes;

        [[nodiscard]] std::string kovaaksDir() const override { return kovaaks_dir; }

        void setKovaaksDir(const std::string &dir) override {
            kovaaks_dir = dir;
            kovaaks_dir_writes.push_back(dir);
            notifyKovaaksDirChanged();
        }

        [[nodiscard]] bool isKovaaksDirValid() const override { return kovaaks_dir_valid; }

        void onKovaaksDirChanged(std::function<void()> callback) override {
            kovaaks_dir_callbacks.push_back(std::move(callback));
        }

        [[nodiscard]] std::string profilePath() const override { return profile_path; }

        void setProfilePath(const std::string &path) override {
            profile_path = path;
            profile_path_writes.push_back(path);
            for (const auto &callback: profile_path_callbacks) callback();
        }

        void onProfilePathChanged(std::function<void()> callback) override {
            profile_path_callbacks.push_back(std::move(callback));
        }

        [[nodiscard]] bool isProfileLoaded() const override { return profile_loaded; }

        void onProfileChanged(std::function<void()> callback) override {
            profile_callbacks.push_back(std::move(callback));
        }

        void notifyKovaaksDirChanged() const {
            for (const auto &callback: kovaaks_dir_callbacks) callback();
        }

        void notifyProfileChanged() const {
            for (const auto &callback: profile_callbacks) callback();
        }

    private:
        std::vector<std::function<void()>> kovaaks_dir_callbacks;
        std::vector<std::function<void()>> profile_path_callbacks;
        std::vector<std::function<void()>> profile_callbacks;
    };
}

#endif
