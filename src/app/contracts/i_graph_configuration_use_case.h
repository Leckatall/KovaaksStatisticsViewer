#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>

#include "graph_configuration.h"

namespace ksv::application {
    // One graph's immediate change from its quick popup. Absent fields keep their committed values.
    struct GraphQuickPatch {
        std::optional<RangePreference> range;
        std::optional<int> averagingWindowDays;
        std::optional<ConnectionMode> connection;
        std::optional<bool> lineModeMarkers;
        // Replaces each named series' override; an empty override restores that series' defaults.
        std::map<std::string, SeriesStyleOverride, std::less<>> seriesStyles;
        std::map<std::string, AxisPolicy, std::less<>> axisPolicies;
    };

    class IGraphConfigurationUseCase {
    public:
        virtual ~IGraphConfigurationUseCase() = default;

        [[nodiscard]] virtual GraphConfigurationSnapshot current() const = 0;

        // Published after a whole accepted replacement; never for a rejected or unchanged candidate.
        virtual void onChanged(std::function<void(const GraphConfigurationChange &)> callback) = 0;

        // Validates the complete candidate, every graph included. Acceptance means the write was
        // submitted, not that it was verified.
        virtual GraphConfigurationSaveResult save(const GraphConfiguration &candidate) = 0;

        // Validates the patched graph before replacing anything; an invalid patch changes nothing.
        virtual GraphConfigurationSaveResult applyQuickPatch(GraphKey graph, const GraphQuickPatch &patch) = 0;
    };
}
