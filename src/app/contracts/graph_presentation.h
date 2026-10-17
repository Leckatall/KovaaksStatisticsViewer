#pragma once

#include <map>
#include <string>
#include <string_view>

#include "graph_configuration.h"

namespace ksv::application {
    // The committed preferences that only change how a graph's data is drawn. A change to any of them
    // re-styles the graph without touching its measurements, range or window.
    struct GraphPresentation {
        GraphKey graph = GraphKey::Run;
        LineAppearance appearance;
        std::map<std::string, SeriesStyleOverride, std::less<>> seriesStyles;
        std::map<std::string, AxisPolicy, std::less<>> axisPolicies;

        [[nodiscard]] SeriesStyle seriesStyle(const std::string_view seriesKey) const {
            return resolvedSeriesStyle(graph, seriesStyles, seriesKey);
        }

        bool operator==(const GraphPresentation &) const = default;
    };

    [[nodiscard]] GraphPresentation presentationOf(GraphKey graph, const GraphPreferences &preferences);

    [[nodiscard]] GraphPresentation defaultGraphPresentation(GraphKey graph);
}
