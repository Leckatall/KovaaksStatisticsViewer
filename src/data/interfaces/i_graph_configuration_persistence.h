#pragma once

#include "app/contracts/graph_configuration.h"

namespace ksv::data {
    // Stateless persistence port for the graph-configuration document. It neither validates nor
    // retains what it loads or saves: the accepted snapshot and its validation belong to the caller.
    class IGraphConfigurationPersistence {
    public:
        virtual ~IGraphConfigurationPersistence() = default;

        // Never fails: an absent document is seeded and an invalid one is quarantined and recovered,
        // both of which are written back.
        [[nodiscard]] virtual application::GraphConfiguration load() = 0;

        // Submits the whole document. Durability is not verified (EDGE-05).
        virtual void save(const application::GraphConfiguration &configuration) = 0;
    };
}
