#ifndef KOVAAKSSTATSVIEWER_GRAPH_COLUMN_PREFERENCES_H
#define KOVAAKSSTATSVIEWER_GRAPH_COLUMN_PREFERENCES_H

#include <memory>

#include "data/interfaces/i_graph_line_config.h"
#include "i_graph_column_preferences.h"

namespace ksv::application {
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
