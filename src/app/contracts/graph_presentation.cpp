#include "graph_presentation.h"

namespace ksv::application {
    ResolvedSeriesStyle GraphPresentation::seriesStyle(const std::string_view seriesKey) const {
        const auto base = defaultSeriesStyle(graph, seriesKey);
        const auto found = seriesStyles.find(seriesKey);
        if (found == seriesStyles.end()) return {base.color, base.width};
        return {found->second.color ? found->second.color : base.color, found->second.width.value_or(base.width)};
    }

    GraphPresentation presentationOf(const GraphKey graph, const GraphPreferences &preferences) {
        return {graph, preferences.appearance, preferences.seriesStyles, preferences.axisPolicies};
    }

    GraphPresentation defaultGraphPresentation(const GraphKey graph) {
        return presentationOf(graph, defaultGraphPreferences(graph));
    }
}
