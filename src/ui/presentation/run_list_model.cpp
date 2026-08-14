//
// Created by Lecka on 09/08/2026.
//

#include "run_list_model.h"

namespace ksv::presentation {
    RunListModel::RunListModel(QObject *parent) : QAbstractListModel(parent) {
    }

    int RunListModel::rowCount(const QModelIndex &parent) const {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_runs.size());
    }

    QVariant RunListModel::data(const QModelIndex &index, const int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_runs.size()))
            return {};

        const auto &run = m_runs[index.row()];
        switch (role) {
            case HashRole: return QString::fromStdString(run.run_id.scenario_id.hash);
            case RunLabelRole: return QString::fromStdString(run.run_id.toString());
            case StartTimeMsRole: return static_cast<qint64>(run.run_id.start_time);
            case ScoreRole: return run.completion.score;
            case AccuracyRole: return run.completion.accuracy();
            case ShotsRole: return run.completion.shots;
            case HitsRole: return run.completion.hits;
            default: return {};
        }
    }

    QHash<int, QByteArray> RunListModel::roleNames() const {
        return {
            {HashRole, "hash"},
            {RunLabelRole, "runLabel"},
            {StartTimeMsRole, "startTimeMs"},
            {ScoreRole, "score"},
            {AccuracyRole, "accuracy"},
            {ShotsRole, "shots"},
            {HitsRole, "hits"},
        };
    }

    void RunListModel::setRuns(std::vector<domain::RunPerformance> runs) {
        beginResetModel();
        m_runs = std::move(runs);
        endResetModel();
        emit countChanged();
    }
}
