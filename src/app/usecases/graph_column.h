//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H
#define KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H

namespace ksv::application {
    // Maps app derivation (perf data) to UI presentation (colors, names). Order must match GraphViewModel::Column.
    enum class ColumnId { Time, Score, Accuracy, Shots, Kills, Dmg, ScoreTotal, ExpectedFinalScore, ExpectedFinalScoreRecent };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H
