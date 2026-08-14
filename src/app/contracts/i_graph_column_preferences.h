#ifndef KOVAAKSSTATSVIEWER_I_GRAPH_COLUMN_PREFERENCES_H
#define KOVAAKSSTATSVIEWER_I_GRAPH_COLUMN_PREFERENCES_H

#include <vector>

#include "graph_column.h"

namespace ksv::application {
    class IGraphColumnPreferences {
    public:
        virtual ~IGraphColumnPreferences() = default;

        [[nodiscard]] virtual std::vector<ColumnId> getEnabledColumns() const = 0;
        [[nodiscard]] virtual bool isEnabled(ColumnId column) const = 0;
        virtual void setEnabled(ColumnId column, bool enabled) = 0;
    };
}

#endif //KOVAAKSSTATSVIEWER_I_GRAPH_COLUMN_PREFERENCES_H
