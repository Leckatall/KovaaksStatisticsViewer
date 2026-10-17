#include "graph_configuration_use_case.h"

#include <utility>

namespace ksv::application {
    GraphConfigurationUseCase::GraphConfigurationUseCase(std::shared_ptr<data::IGraphConfigurationService> service)
        : m_service(std::move(service)) {
    }

    GraphConfigurationSnapshot GraphConfigurationUseCase::current() const { return m_service->snapshot(); }

    void GraphConfigurationUseCase::onChanged(std::function<void(const GraphConfigurationChange &)> callback) {
        m_service->onChanged(std::move(callback));
    }

    GraphConfigurationSaveResult GraphConfigurationUseCase::save(const GraphConfiguration &candidate) {
        return m_service->replace(GraphConfiguration(candidate));
    }

    GraphConfigurationSaveResult GraphConfigurationUseCase::applyQuickPatch(const GraphKey graph,
                                                                            const GraphQuickPatch &patch) {
        const auto snapshot = m_service->snapshot();
        auto candidate = *snapshot.configuration;
        auto &preferences = candidate.preferences(graph);
        if (patch.range) preferences.range = patch.range;
        if (patch.averagingWindowDays) preferences.averagingWindowDays = patch.averagingWindowDays;
        if (patch.connection) preferences.appearance.connection = *patch.connection;
        if (patch.lineModeMarkers) preferences.appearance.lineModeMarkers = *patch.lineModeMarkers;
        for (const auto &[series, style]: patch.seriesStyles) {
            if (style == SeriesStyleOverride{}) preferences.seriesStyles.erase(series);
            else preferences.seriesStyles.insert_or_assign(series, style);
        }
        for (const auto &[axis, policy]: patch.axisPolicies) preferences.axisPolicies.insert_or_assign(axis, policy);

        if (auto issues = validateGraphPreferences(graph, preferences); !issues.empty())
            return {std::move(issues), snapshot.revision};
        return m_service->replace(std::move(candidate));
    }
}
