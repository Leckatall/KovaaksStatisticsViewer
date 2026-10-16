#pragma once

#include <memory>

#include "data/interfaces/i_graph_configuration_persistence.h"
#include "data/interfaces/i_settings_service.h"

namespace ksv::qt_data {
    class GraphConfigurationPersistence final : public data::IGraphConfigurationPersistence {
    public:
        explicit GraphConfigurationPersistence(std::shared_ptr<application::ISettingsService> settingsService);

        [[nodiscard]] application::GraphConfiguration load() override;

        void save(const application::GraphConfiguration &configuration) override;

    private:
        std::shared_ptr<application::ISettingsService> m_settingsService;
    };
}
