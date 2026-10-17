#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "contracts/i_recent_benchmarks.h"
#include "data/interfaces/i_settings_service.h"

namespace ksv::application {
    class RecentBenchmarks final : public IRecentBenchmarks {
    public:
        explicit RecentBenchmarks(std::shared_ptr<ISettingsService> settings);

        [[nodiscard]] const std::vector<std::string> &ids() const override { return m_ids; }
        void markViewed(const std::string &id) override;
        void markCreated(const std::string &id) override;
        void onChanged(std::function<void()> callback) override { m_callbacks.push_back(std::move(callback)); }

    private:
        void commit(std::vector<std::string> next);

        std::shared_ptr<ISettingsService> m_settings;
        std::vector<std::string> m_ids;
        std::vector<std::function<void()> > m_callbacks;
    };
}
