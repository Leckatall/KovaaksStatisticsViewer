#ifndef KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_GRAPH_CONFIGURATION_PERSISTENCE_H
#define KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_GRAPH_CONFIGURATION_PERSISTENCE_H

#include <vector>

#include "data/interfaces/i_graph_configuration_persistence.h"

namespace ksv::tests_support {
    class FakeGraphConfigurationPersistence final : public data::IGraphConfigurationPersistence {
    public:
        application::GraphConfiguration stored = application::defaultGraphConfiguration();
        std::vector<application::GraphConfiguration> saves;
        int loads = 0;

        [[nodiscard]] application::GraphConfiguration load() override {
            ++loads;
            return stored;
        }

        void save(const application::GraphConfiguration &configuration) override {
            saves.push_back(configuration);
            stored = configuration;
        }
    };
}

#endif
