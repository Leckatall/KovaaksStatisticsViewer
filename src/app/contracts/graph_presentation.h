#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "graph_configuration.h"

namespace ksv::application {
    struct ResolvedSeriesStyle {
        // Absent: drawn in the theme's foreground colour.
        std::optional<RgbaColor> color;
        double width = kDefaultSeriesWidth;

        bool operator==(const ResolvedSeriesStyle &) const = default;
    };

    // The committed preferences that only change how a graph's data is drawn. A change to any of them
    // re-styles the graph without touching its measurements, range or window.
    struct GraphPresentation {
        GraphKey graph = GraphKey::Run;
        LineAppearance appearance;
        std::map<std::string, SeriesStyleOverride, std::less<>> seriesStyles;
        std::map<std::string, AxisPolicy, std::less<>> axisPolicies;

        // The graph's default for `seriesKey` with the committed override laid over it.
        [[nodiscard]] ResolvedSeriesStyle seriesStyle(std::string_view seriesKey) const;

        bool operator==(const GraphPresentation &) const = default;
    };

    [[nodiscard]] GraphPresentation presentationOf(GraphKey graph, const GraphPreferences &preferences);

    [[nodiscard]] GraphPresentation defaultGraphPresentation(GraphKey graph);
}
