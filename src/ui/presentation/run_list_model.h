//
// Created by Lecka on 09/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_RUN_LIST_MODEL_H
#define KOVAAKSSTATSVIEWER_RUN_LIST_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <vector>

#include "app/usecases/run_summary.h"

namespace ksv::presentation {
    class RunListModel : public QAbstractListModel {
        Q_OBJECT
        // QAbstractListModel has no built-in "count" property (unlike QML's
        // ListModel); RunListView.qml reads runModel.count directly.
        Q_PROPERTY(int count READ count NOTIFY countChanged)

    public:
        enum Role {
            HashRole = Qt::UserRole + 1,
            RunLabelRole,
            ScenarioNameRole,
            StartTimeMsRole,
            ScoreRole,
            AccuracyRole,
            DurationSecondsRole,
            ShotsRole,
            HitsRole,
        };

        explicit RunListModel(QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] int count() const { return rowCount(); }

        [[nodiscard]] const application::RunSummary *runAt(int row) const;

        void setRuns(std::vector<application::RunSummary> runs);

    signals:
        void countChanged();

    private:
        std::vector<application::RunSummary> m_runs;
    };
}

#endif //KOVAAKSSTATSVIEWER_RUN_LIST_MODEL_H
