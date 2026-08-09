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
            case ScenarioNameRole: return run.scenario_name;
            case StartTimeMsRole: return run.start_time_ms;
            case ScoreRole: return run.score;
            case AccuracyRole: return run.accuracy;
            case DurationSecondsRole: return run.duration_seconds;
            case ShotsRole: return run.shots;
            case HitsRole: return run.hits;
            default: return {};
        }
    }

    QHash<int, QByteArray> RunListModel::roleNames() const {
        return {
            {HashRole, "hash"},
            {RunLabelRole, "runLabel"},
            {ScenarioNameRole, "scenarioName"},
            {StartTimeMsRole, "startTimeMs"},
            {ScoreRole, "score"},
            {AccuracyRole, "accuracy"},
            {DurationSecondsRole, "durationSeconds"},
            {ShotsRole, "shots"},
            {HitsRole, "hits"},
        };
    }

    const application::RunSummary *RunListModel::runAt(const int row) const {
        if (row < 0 || row >= static_cast<int>(m_runs.size())) return nullptr;
        return &m_runs[row];
    }

    void RunListModel::setRuns(std::vector<application::RunSummary> runs) {
        beginResetModel();
        m_runs = std::move(runs);
        endResetModel();
        emit countChanged();
    }
}
