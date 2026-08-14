#include <gtest/gtest.h>

#include <array>
#include <string_view>

#include "app/contracts/graph_column.h"

using namespace ksv::application;

namespace {
    TEST(GraphColumnTest, StableKeysMatchPersistentContract) {
        constexpr std::array expected{
            std::pair{ColumnId::Score, std::string_view{"score"}},
            std::pair{ColumnId::Accuracy, std::string_view{"accuracy"}},
            std::pair{ColumnId::Shots, std::string_view{"shots"}},
            std::pair{ColumnId::Kills, std::string_view{"kills"}},
            std::pair{ColumnId::Dmg, std::string_view{"dmg"}},
            std::pair{ColumnId::ScoreTotal, std::string_view{"scoreTotal"}},
            std::pair{ColumnId::ExpectedFinalScore, std::string_view{"expectedFinalScore"}},
            std::pair{ColumnId::ExpectedFinalScoreRecent, std::string_view{"expectedFinalScoreRecent"}}
        };

        for (const auto &[column, key]: expected) EXPECT_EQ(graphColumnKey(column), key);
    }

    TEST(GraphColumnTest, StableKeysRoundTrip) {
        for (const auto column: kPlottableColumnIds) {
            EXPECT_EQ(graphColumnIdFromKey(graphColumnKey(column)), column);
        }
        EXPECT_EQ(graphColumnIdFromKey(graphColumnKey(ColumnId::Time)), ColumnId::Time);
    }

    TEST(GraphColumnTest, UnknownKeyIsRejected) {
        EXPECT_FALSE(graphColumnIdFromKey("futureColumn").has_value());
        EXPECT_FALSE(graphColumnIdFromKey("").has_value());
    }

    TEST(GraphColumnTest, PlottableColumnsExcludeTimeAndPreserveDisplayOrder) {
        constexpr std::array expected{
            ColumnId::Score,
            ColumnId::Accuracy,
            ColumnId::Shots,
            ColumnId::Kills,
            ColumnId::Dmg,
            ColumnId::ScoreTotal,
            ColumnId::ExpectedFinalScore,
            ColumnId::ExpectedFinalScoreRecent
        };

        EXPECT_EQ(kPlottableColumnIds, expected);
    }
}
