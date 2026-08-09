//
// Created by Lecka on 09/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SCENARIO_LIST_MODEL_H
#define KOVAAKSSTATSVIEWER_SCENARIO_LIST_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <vector>

#include "app/usecases/run_summary.h"

namespace ksv::presentation {
    class ScenarioListModel : public QAbstractListModel {
        Q_OBJECT
        // QAbstractListModel has no built-in "count" property (unlike QML's
        // ListModel); ScenarioSearchPanel.qml reads scenarioModel.count directly.
        Q_PROPERTY(int count READ count NOTIFY countChanged)

    public:
        enum Role {
            NameRole = Qt::UserRole + 1,
            HashRole,
            RunCountRole,
            TotalTimeRole,
        };

        explicit ScenarioListModel(QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] int count() const { return rowCount(); }

        void setSummaries(std::vector<application::ScenarioSummary> summaries);

    signals:
        void countChanged();

    private:
        std::vector<application::ScenarioSummary> m_summaries;
    };
}

#endif //KOVAAKSSTATSVIEWER_SCENARIO_LIST_MODEL_H
