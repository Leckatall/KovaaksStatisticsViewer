//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H
#define KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H

#include <array>
#include <optional>
#include <string_view>

namespace ksv::application {
    // Numeric values remain index-aligned with GraphViewModel::Column.
    enum class ColumnId {
        Time = 0,
        Score = 1,
        Accuracy = 2,
        Shots = 3,
        Kills = 4,
        Dmg = 5,
        ScoreTotal = 6,
        ExpectedFinalScore = 7,
        ExpectedFinalScoreRecent = 8
    };

    inline constexpr std::array kPlottableColumnIds{
        ColumnId::Score,
        ColumnId::Accuracy,
        ColumnId::Shots,
        ColumnId::Kills,
        ColumnId::Dmg,
        ColumnId::ScoreTotal,
        ColumnId::ExpectedFinalScore,
        ColumnId::ExpectedFinalScoreRecent
    };

    // Persisted keys must never be renamed or reused for a different column.
    [[nodiscard]] constexpr std::string_view graphColumnKey(const ColumnId column) {
        switch (column) {
            case ColumnId::Time: return "time";
            case ColumnId::Score: return "score";
            case ColumnId::Accuracy: return "accuracy";
            case ColumnId::Shots: return "shots";
            case ColumnId::Kills: return "kills";
            case ColumnId::Dmg: return "dmg";
            case ColumnId::ScoreTotal: return "scoreTotal";
            case ColumnId::ExpectedFinalScore: return "expectedFinalScore";
            case ColumnId::ExpectedFinalScoreRecent: return "expectedFinalScoreRecent";
        }
        return {};
    }

    [[nodiscard]] constexpr std::optional<ColumnId> graphColumnIdFromKey(const std::string_view key) {
        constexpr std::array allColumns{
            ColumnId::Time,
            ColumnId::Score,
            ColumnId::Accuracy,
            ColumnId::Shots,
            ColumnId::Kills,
            ColumnId::Dmg,
            ColumnId::ScoreTotal,
            ColumnId::ExpectedFinalScore,
            ColumnId::ExpectedFinalScoreRecent
        };
        for (const auto column: allColumns) {
            if (graphColumnKey(column) == key) return column;
        }
        return std::nullopt;
    }
}

#endif //KOVAAKSSTATSVIEWER_GRAPH_COLUMN_H
