#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "formats/csv/stats_csv_parser.h"

namespace {
    class StatsCsvParserTest : public ::testing::Test {
    protected:
        std::filesystem::path fixture(const std::string_view filename) const {
            return std::filesystem::path(TEST_FILES_DIR) / filename;
        }

        ksv::data::StatsCsvParser parser;
    };

    TEST_F(StatsCsvParserTest, ParsesFooterTotalsIdentityAndStats) {
        const auto parsed = parser.parseFile(fixture("1wall6targets TE.csv"));

        ASSERT_TRUE(parsed);
        EXPECT_EQ(parsed->scenario_id.name, "1wall6targets TE");
        EXPECT_EQ(parsed->scenario_id.hash, "3e50391f3c3f484c10a4b8fb362ded17");
        EXPECT_FLOAT_EQ(parsed->totals.score, 153.088882F);
        EXPECT_EQ(parsed->totals.shots, 180);
        EXPECT_EQ(parsed->totals.hits, 166);
        EXPECT_EQ(parsed->totals.misses, 14);
        EXPECT_EQ(parsed->totals.kills, 166);
        EXPECT_EQ(parsed->stats.sens_scale, "Valorant");
        EXPECT_FLOAT_EQ(parsed->stats.horiz_sens, 0.16F);
        EXPECT_EQ(parsed->stats.dpi, 1600);
        EXPECT_FLOAT_EQ(parsed->stats.resolution_scale, 100.0F);
    }

    TEST_F(StatsCsvParserTest, IgnoresUnknownFooterKeysAndDefaultsMissingSettings) {
        const auto path = std::filesystem::temp_directory_path() / "ksv_stats_csv_parser_defaults.csv";
        {
            std::ofstream output(path);
            output << "weapon summary\n\n"
                   << "Scenario:, Fixture\n"
                   << "Hash:, fixture-hash\n"
                   << "Challenge Start:, 01:02:03.004\n"
                   << "Score:, 42.5\n"
                   << "Hit Count:, 4\n"
                   << "Miss Count:, 1\n"
                   << "Kills:, 3\n"
                   << "Future Field:, ignored\n";
        }

        const auto parsed = parser.parseFile(path);
        std::filesystem::remove(path);

        ASSERT_TRUE(parsed);
        EXPECT_EQ(parsed->totals.shots, 5);
        EXPECT_EQ(parsed->stats.sens_scale, "");
        EXPECT_EQ(parsed->stats.dpi, 0);
        EXPECT_EQ(parsed->challenge_start, std::chrono::hours{1} + std::chrono::minutes{2} +
                                                  std::chrono::seconds{3} + std::chrono::milliseconds{4});
    }

    TEST_F(StatsCsvParserTest, RejectsFooterWithoutHash) {
        const auto path = std::filesystem::temp_directory_path() / "ksv_stats_csv_parser_missing_hash.csv";
        {
            std::ofstream output(path);
            output << "weapon summary\n\n"
                   << "Scenario:, Fixture\n"
                   << "Challenge Start:, 01:02:03.004\n"
                   << "Score:, 42.5\n"
                   << "Hit Count:, 4\n"
                   << "Miss Count:, 1\n"
                   << "Kills:, 3\n";
        }

        EXPECT_FALSE(parser.parseFile(path));
        std::filesystem::remove(path);
    }
}
