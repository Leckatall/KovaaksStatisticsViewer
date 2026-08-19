#ifndef KOVAAKSSTATSVIEWER_GRAPH_COLUMN_PREFERENCES_H
#define KOVAAKSSTATSVIEWER_GRAPH_COLUMN_PREFERENCES_H

#include <memory>

#include "data/interfaces/i_graph_line_config.h"
#include "contracts/i_graph_column_preferences.h"

namespace ksv::application {
    // TODO(2026-08-19): Orphaned — nothing in src/ or tools/ constructs this anymore. Superseded by
    // SeriesConfigStore's per-series `enabled` flag. Safe to delete along with IGraphColumnPreferences
    // and IGraphLineConfig once nothing references them.
    class GraphColumnPreferences final : public IGraphColumnPreferences {
    public:
        explicit GraphColumnPreferences(std::shared_ptr<IGraphLineConfig> config);

        [[nodiscard]] std::vector<ColumnId> getEnabledColumns() const override;
        [[nodiscard]] bool isEnabled(ColumnId column) const override;
        void setEnabled(ColumnId column, bool enabled) override;

    private:
        std::shared_ptr<IGraphLineConfig> m_config;
    };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_COLUMN_PREFERENCES_H
