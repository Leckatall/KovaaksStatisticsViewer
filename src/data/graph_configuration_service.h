#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "data/interfaces/i_graph_configuration_persistence.h"
#include "data/interfaces/i_graph_configuration_service.h"

namespace ksv::data {
    class GraphConfigurationService final : public IGraphConfigurationService {
    public:
        explicit GraphConfigurationService(std::shared_ptr<IGraphConfigurationPersistence> persistence);

        [[nodiscard]] application::GraphConfigurationSnapshot snapshot() const override;

        application::GraphConfigurationSaveResult replace(application::GraphConfiguration candidate) override;

        void onChanged(std::function<void(const application::GraphConfigurationChange &)> callback) override;

    private:
        void publish(application::GraphConfigurationChange change);

        std::shared_ptr<IGraphConfigurationPersistence> m_persistence;
        mutable std::mutex m_mutex;
        std::shared_ptr<const application::GraphConfiguration> m_configuration;
        std::uint64_t m_revision = 0;
        std::vector<std::function<void(const application::GraphConfigurationChange &)>> m_callbacks;
        std::deque<application::GraphConfigurationChange> m_unpublished;
        bool m_publishing = false;
    };
}
