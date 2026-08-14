//
// Created by Lecka on 09/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_RUN_LIST_MODEL_H
#define KOVAAKSSTATSVIEWER_RUN_LIST_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <vector>

#include "domain/run_performance.h"

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
            StartTimeMsRole,
            ScoreRole,
            AccuracyRole,
            ShotsRole,
            HitsRole,
        };

        explicit RunListModel(QObject *parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] int count() const { return rowCount(); }

        void setRuns(std::vector<domain::RunPerformance> runs);

    signals:
        void countChanged();

    private:
        std::vector<domain::RunPerformance> m_runs;
    };
}

#endif //KOVAAKSSTATSVIEWER_RUN_LIST_MODEL_H
