#pragma once

#include <functional>

#include "app/contracts/graph_configuration.h"

namespace ksv::data {
    // Qt-free authority over the committed graph configuration: the single accepted snapshot, its
    // in-process revision and coherent change publication. It holds no editor working copy.
    class IGraphConfigurationService {
    public:
        virtual ~IGraphConfigurationService() = default;

        // Safe to call from any thread; replace() and onChanged() belong to one thread.
        [[nodiscard]] virtual application::GraphConfigurationSnapshot snapshot() const = 0;

        // Validates the whole candidate. Rejection or an unchanged candidate replaces, writes and
        // publishes nothing.
        virtual application::GraphConfigurationSaveResult replace(const application::GraphConfiguration &candidate) = 0;

        // Called after the replacement is installed, in revision order even when a callback replaces again.
        virtual void onChanged(std::function<void(const application::GraphConfigurationChange &)> callback) = 0;
    };
}
