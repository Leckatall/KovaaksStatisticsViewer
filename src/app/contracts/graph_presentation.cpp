#include "graph_presentation.h"

namespace ksv::application {
    GraphPresentation presentationOf(const GraphKey graph, const GraphPreferences &preferences) {
        return {graph, preferences.appearance, preferences.seriesStyles, preferences.axisPolicies};
    }

    GraphPresentation defaultGraphPresentation(const GraphKey graph) {
        return presentationOf(graph, defaultGraphPreferences(graph));
    }
}
