#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_SETTINGS_SERVICE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_SETTINGS_SERVICE_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "data/interfaces/i_settings_service.h"

namespace ksv::tests_support {
    class FakeSettingsService final : public application::ISettingsService {
    public:
        std::vector<std::string> dirs;
        std::string profile_path;
        std::optional<std::string> document;
        std::vector<std::string> quarantined;
        std::vector<std::string> legacyDisabledColumns;
        int document_writes = 0;

        [[nodiscard]] std::vector<std::string> getKovaaksDirs() const override { return dirs; }
        [[nodiscard]] bool isKovaaksDirSet() const override { return !dirs.empty(); }

        void setKovaaksDirs(const std::vector<std::string> &new_dirs) override {
            dirs = new_dirs;
            for (const auto &callback: kovaaks_dirs_callbacks) callback();
        }

        [[nodiscard]] std::string getProfilePath() const override { return profile_path; }

        void setProfilePath(const std::string &new_path) override {
            profile_path = new_path;
            for (const auto &callback: profile_path_callbacks) callback();
        }

        void onProfilePathChanged(std::function<void()> callback) override {
            profile_path_callbacks.push_back(std::move(callback));
        }

        void onKovaaksDirsChanged(std::function<void()> callback) override {
            kovaaks_dirs_callbacks.push_back(std::move(callback));
        }

        [[nodiscard]] bool hasSeriesConfigDocument() const override { return document.has_value(); }
        [[nodiscard]] std::string getSeriesConfigDocument() const override { return document.value_or(""); }

        void setSeriesConfigDocument(const std::string &json) override {
            document = json;
            ++document_writes;
        }

        void quarantineSeriesConfigDocument(const std::string &invalid_json) override {
            quarantined.push_back(invalid_json);
        }

        [[nodiscard]] std::vector<std::string> getLegacyDisabledColumnKeys() const override {
            return legacyDisabledColumns;
        }

    private:
        std::vector<std::function<void()>> kovaaks_dirs_callbacks;
        std::vector<std::function<void()>> profile_path_callbacks;
    };
}

#endif
