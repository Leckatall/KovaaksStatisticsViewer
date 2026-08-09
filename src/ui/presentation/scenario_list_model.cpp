//
// Created by Lecka on 09/08/2026.
//

#include "scenario_list_model.h"

namespace ksv::presentation {
    ScenarioListModel::ScenarioListModel(QObject *parent) : QAbstractListModel(parent) {
    }

    int ScenarioListModel::rowCount(const QModelIndex &parent) const {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_summaries.size());
    }

    QVariant ScenarioListModel::data(const QModelIndex &index, const int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_summaries.size()))
            return {};

        const auto &summary = m_summaries[index.row()];
        switch (role) {
            case NameRole: return QString::fromStdString(summary.scenario_id.name);
            case HashRole: return QString::fromStdString(summary.scenario_id.hash);
            case RunCountRole: return summary.run_count;
            case TotalTimeRole: return summary.total_time_seconds;
            default: return {};
        }
    }

    QHash<int, QByteArray> ScenarioListModel::roleNames() const {
        return {
            {NameRole, "name"},
            {HashRole, "hash"},
            {RunCountRole, "runCount"},
            {TotalTimeRole, "totalTime"},
        };
    }

    void ScenarioListModel::setSummaries(std::vector<application::ScenarioSummary> summaries) {
        beginResetModel();
        m_summaries = std::move(summaries);
        endResetModel();
        emit countChanged();
    }
}
