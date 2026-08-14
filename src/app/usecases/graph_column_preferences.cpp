#include "graph_column_preferences.h"

#include <algorithm>
#include <unordered_set>

namespace ksv::application {
    GraphColumnPreferences::GraphColumnPreferences(std::shared_ptr<IGraphLineConfig> config)
        : m_config(std::move(config)) {}

    std::vector<ColumnId> GraphColumnPreferences::getEnabledColumns() const {
        const auto rawDisabled = m_config->getDisabledGraphLineKeys();
        const std::unordered_set<std::string> disabled(rawDisabled.begin(), rawDisabled.end());
        std::vector<ColumnId> result;
        result.reserve(kPlottableColumnIds.size());
        for (const auto column: kPlottableColumnIds) {
            if (!disabled.contains(std::string(graphColumnKey(column)))) result.push_back(column);
        }
        return result;
    }

    bool GraphColumnPreferences::isEnabled(const ColumnId column) const {
        if (!isPlottableGraphColumn(column)) return false;
        const auto disabled = m_config->getDisabledGraphLineKeys();
        const auto key = graphColumnKey(column);
        return std::ranges::none_of(disabled, [key](const auto &value) { return value == key; });
    }

    void GraphColumnPreferences::setEnabled(const ColumnId column, const bool enabled) {
        if (!isPlottableGraphColumn(column)) return;

        auto disabled = m_config->getDisabledGraphLineKeys();
        const std::string key(graphColumnKey(column));
        const bool currentlyDisabled = std::ranges::find(disabled, key) != disabled.end();
        if (enabled == !currentlyDisabled) return;

        if (enabled) {
            std::erase(disabled, key);
        } else {
            disabled.push_back(key);
        }
        m_config->setDisabledGraphLineKeys(disabled);
    }
}
