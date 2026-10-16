#ifndef KOVAAKSSTATISTICSVIEWER_GRAPH_PRESENTATION_SOURCE_H
#define KOVAAKSSTATISTICSVIEWER_GRAPH_PRESENTATION_SOURCE_H

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "contracts/graph_presentation.h"
#include "contracts/i_graph_configuration_use_case.h"

namespace ksv::application {
    // One graph's committed style, announced only when that graph's style section changes. Pinned in
    // place: the configuration subscription holds `this`.
    class GraphPresentationSource {
    public:
        GraphPresentationSource(std::shared_ptr<IGraphConfigurationUseCase> configuration, const GraphKey graph)
            : m_configuration(std::move(configuration)), m_graph(graph) {
            if (!m_configuration) return;
            m_configuration->onChanged([this](const GraphConfigurationChange &change) {
                if (!change.sectionsOf(m_graph).style) return;
                for (const auto &callback: m_callbacks) callback();
            });
        }

        GraphPresentationSource(const GraphPresentationSource &) = delete;
        GraphPresentationSource &operator=(const GraphPresentationSource &) = delete;

        [[nodiscard]] GraphPresentation current() const {
            if (!m_configuration) return defaultGraphPresentation(m_graph);
            return presentationOf(m_graph, m_configuration->current().configuration->preferences(m_graph));
        }

        void onChanged(std::function<void()> callback) { m_callbacks.push_back(std::move(callback)); }

    private:
        std::shared_ptr<IGraphConfigurationUseCase> m_configuration;
        GraphKey m_graph;
        std::vector<std::function<void()>> m_callbacks;
    };
}

#endif //KOVAAKSSTATISTICSVIEWER_GRAPH_PRESENTATION_SOURCE_H
