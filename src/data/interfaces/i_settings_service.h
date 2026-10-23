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
        virtual void setKovaaksDirs(const std::vector<std::string> &dirs) = 0;

        [[nodiscard]] virtual std::string getProfilePath() const = 0;
        virtual void setProfilePath(const std::string &path) = 0;

        virtual void onProfilePathChanged(std::function<void()> callback) = 0;
        virtual void onKovaaksDirsChanged(std::function<void()> callback) = 0;

        [[nodiscard]] virtual bool hasSeriesConfigDocument() const = 0;
        [[nodiscard]] virtual std::string getSeriesConfigDocument() const = 0;
        virtual void setSeriesConfigDocument(const std::string &json) = 0;
        virtual void quarantineSeriesConfigDocument(const std::string &invalidJson) = 0;
        [[nodiscard]] virtual std::vector<std::string> getLegacyDisabledColumnKeys() const = 0;
        // The run graph's Y-axis-label series ID as VisualSettingsManager.qml stored it; empty when unset.
        [[nodiscard]] virtual std::string getLegacyRunAxisLabelSeriesId() const = 0;

        // Most recent first; the front is what opens next launch.
        [[nodiscard]] virtual std::vector<std::string> getRecentBenchmarkIds() const = 0;
        virtual void setRecentBenchmarkIds(const std::vector<std::string> &ids) = 0;
    };
}
#endif //KOVAAKSSTATSVIEWER_I_SETTINGS_SERVICE_H

