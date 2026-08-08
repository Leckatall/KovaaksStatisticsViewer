//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H
#define KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H

namespace ksv::application {
    // Identity shared between the app layer (which decides how a column is
    // derived from raw perf data) and the UI layer (which decides how it's
    // named, colored, and formatted). Order matches presentation::GraphViewModel::Column.
    enum class ColumnId { Time, Score, Accuracy, Shots, Kills, Dmg, ScoreTotal, ExpectedFinalScore, ExpectedFinalScoreRecent };
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H
