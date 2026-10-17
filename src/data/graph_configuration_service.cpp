#include "graph_configuration_service.h"

#include <cstddef>
#include <utility>

namespace ksv::data {
    using namespace application;

    GraphConfigurationService::GraphConfigurationService(std::shared_ptr<IGraphConfigurationPersistence> persistence)
        : m_persistence(std::move(persistence)),
          m_configuration(std::make_shared<const GraphConfiguration>(m_persistence->load())),
          m_revision(1) {
    }

    GraphConfigurationSnapshot GraphConfigurationService::snapshot() const {
        std::lock_guard lock(m_mutex);
        return {m_configuration, m_revision};
    }

    GraphConfigurationSaveResult GraphConfigurationService::replace(GraphConfiguration candidate) {
        const auto current = snapshot();
        if (auto issues = validateGraphConfiguration(candidate); !issues.empty())
            return {std::move(issues), current.revision};
        auto change = diffGraphConfigurations(*current.configuration, candidate);
        if (change.empty()) return {{}, current.revision};

        m_persistence->save(candidate);
        auto replacement = std::make_shared<const GraphConfiguration>(std::move(candidate)); {
            std::lock_guard lock(m_mutex);
            m_configuration = std::move(replacement);
            change.revision = ++m_revision;
        }
        const auto revision = change.revision;
        publish(std::move(change));
        return {{}, revision};
    }

    void GraphConfigurationService::onChanged(std::function<void(const GraphConfigurationChange &)> callback) {
        m_callbacks.push_back(std::move(callback));
    }

    void GraphConfigurationService::publish(GraphConfigurationChange change) {
        m_unpublished.push_back(std::move(change));
        if (m_publishing) return;
        m_publishing = true;
        while (!m_unpublished.empty()) {
            const auto next = std::move(m_unpublished.front());
            m_unpublished.pop_front();
            for (const auto & callback : m_callbacks) callback(next);
        }
        m_publishing = false;
    }
}
