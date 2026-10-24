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

    TEST_F(StatsCsvParserTest, ParsesDamageDoneFromFooterNotFromTheWeaponSummaryHeader) {
        const auto parsed = parser.parseFile(fixture("1wall6targets TE.csv"));

        ASSERT_TRUE(parsed);
        ASSERT_TRUE(parsed->totals.damage.has_value());
        EXPECT_FLOAT_EQ(*parsed->totals.damage, 166.0F);
    }

    TEST_F(StatsCsvParserTest, DamageIsAbsentWhenMissingAndNonFatalWhenUnparseable) {
        const auto write = [this](const std::string &damage_line) {
            const auto path = std::filesystem::temp_directory_path() / "ksv_stats_csv_parser_damage.csv";
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
                       << damage_line;
            }
            auto parsed = parser.parseFile(path);
            std::filesystem::remove(path);
            return parsed;
        };

        const auto absent = write("");
        ASSERT_TRUE(absent);
        EXPECT_FALSE(absent->totals.damage.has_value());

        const auto unparseable = write("Damage Done:, lots\n");
        ASSERT_TRUE(unparseable);
        EXPECT_FALSE(unparseable->totals.damage.has_value());
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

    // Pre-2.0.3.0 files carry neither Challenge Start nor hit/miss counts.
    TEST_F(StatsCsvParserTest, Parses2020CsvUsingFilenameEndTimeAndWeaponSummary) {
        const auto parsed = parser.parseFile(fixture("1wall6targets TE - Challenge - 2020.05.29-08.10.33 Stats.csv"));

        ASSERT_TRUE(parsed);
        EXPECT_EQ(parsed->scenario_id.hash, "3e50391f3c3f484c10a4b8fb362ded17");
        EXPECT_EQ(parsed->challenge_start, std::chrono::hours{8} + std::chrono::minutes{10} + std::chrono::seconds{33});
        EXPECT_EQ(parsed->totals.hits, 132);
        EXPECT_EQ(parsed->totals.misses, 26);
        EXPECT_EQ(parsed->totals.shots, 158);
        EXPECT_EQ(parsed->totals.kills, 132);
        EXPECT_FLOAT_EQ(parsed->totals.score, 110.278481F);
    }

    // 2.0.3.0 – 3.0.x files have Challenge Start but still no hit/miss counts.
    TEST_F(StatsCsvParserTest, Parses2021CsvUsingChallengeStartAndWeaponSummary) {
        const auto parsed = parser.parseFile(fixture("1w4ts Voltaic - Challenge - 2021.08.16-07.36.22 Stats.csv"));

        ASSERT_TRUE(parsed);
        EXPECT_EQ(parsed->scenario_id.name, "1w4ts Voltaic");
        EXPECT_EQ(parsed->challenge_start, std::chrono::hours{7} + std::chrono::minutes{35} +
                                                  std::chrono::seconds{5} + std::chrono::milliseconds{846});
        EXPECT_EQ(parsed->totals.hits, 84);
        EXPECT_EQ(parsed->totals.misses, 7);
        EXPECT_EQ(parsed->stats.sens_scale, "Overwatch");
        // Its footer claims 205 of pause inside a 76s run.
        EXPECT_EQ(parsed->pause_count, 0);
        EXPECT_EQ(parsed->pause_duration, std::chrono::milliseconds::zero());
    }

    // 3.0.x writes Challenge Start as a literal placeholder and Pause Duration as INT_MIN.
    TEST_F(StatsCsvParserTest, Parses30xCsvUsingFilenameEndTimeDespitePlaceholderStart) {
        const auto path = std::filesystem::temp_directory_path() / "Fixture - Challenge - 2022.05.09-21.32.06 Stats.csv";
        {
            std::ofstream output(path);
            output << "Kill #,Timestamp\n\n"
                   << "Weapon,Shots,Hits,Damage Done,Damage Possible,,Sens Scale\n"
                   << "BB Gun,141,125,125.0,141.0,\n\n"
                   << "Kills:,125\n"
                   << "Score:,110.815598\n"
                   << "Scenario:,Fixture\n"
                   << "Hash:,fixture-hash\n"
                   << "Game Version:,3.0.2\n"
                   << "Challenge Start:,H:M:S.s\n"
                   << "Pause Count:,1\n"
                   << "Pause Duration:,-2147483648.0\n";
        }

        const auto parsed = parser.parseFile(path);
        std::filesystem::remove(path);

        ASSERT_TRUE(parsed);
        EXPECT_EQ(parsed->challenge_start, std::chrono::hours{21} + std::chrono::minutes{32} + std::chrono::seconds{6});
        EXPECT_EQ(parsed->totals.misses, 16);
        EXPECT_EQ(parsed->pause_duration, std::chrono::milliseconds::zero());
    }

    TEST_F(StatsCsvParserTest, SumsEveryWeaponSummaryRowForLegacyVersions) {
        const auto path = std::filesystem::temp_directory_path() / "Fixture - Challenge - 2020.06.01-12.00.00 Stats.csv";
        {
            std::ofstream output(path);
            output << "Kill #,Timestamp\n\n"
                   << "Weapon,Shots,Hits,Damage Done,Damage Possible\n"
                   << "Rifle,10,6,6.0,10.0,\n"
                   << "Pistol,5,4,4.0,5.0,\n\n"
                   << "Kills:,10\n"
                   << "Score:,42.5\n"
                   << "Scenario:,Fixture\n"
                   << "Hash:,fixture-hash\n"
                   << "Game Version:,v2.0.1.2\n";
        }

        const auto parsed = parser.parseFile(path);
        std::filesystem::remove(path);

        ASSERT_TRUE(parsed);
        EXPECT_EQ(parsed->totals.hits, 10);
        EXPECT_EQ(parsed->totals.misses, 5);
        EXPECT_EQ(parsed->challenge_start, std::chrono::hours{12});
    }

    // The legacy path is gated on version, not key absence: a current file missing its
    // hit counts is malformed, not legacy, even though a weapon summary is present.
    TEST_F(StatsCsvParserTest, RejectsCurrentVersionWithoutHitCountsDespiteWeaponSummary) {
        const auto path = std::filesystem::temp_directory_path() / "Fixture - Challenge - 2026.06.01-12.00.00 Stats.csv";
        {
            std::ofstream output(path);
            output << "Kill #,Timestamp\n\n"
                   << "Weapon,Shots,Hits,Damage Done,Damage Possible\n"
                   << "Rifle,10,6,6.0,10.0,\n\n"
                   << "Kills:,6\n"
                   << "Score:,42.5\n"
                   << "Scenario:,Fixture\n"
                   << "Hash:,fixture-hash\n"
                   << "Game Version:,3.9.10.2026-09-28-11-58-12-6935f347dcca\n"
                   << "Challenge Start:,11:59:00.000\n";
        }

        EXPECT_FALSE(parser.parseFile(path));
        std::filesystem::remove(path);
    }

    TEST_F(StatsCsvParserTest, RejectsLegacyVersionWithoutWeaponSummary) {
        const auto path = std::filesystem::temp_directory_path() / "Fixture - Challenge - 2021.06.01-12.00.00 Stats.csv";
        {
            std::ofstream output(path);
            output << "Kill #,Timestamp\n\n"
                   << "Kills:,6\n"
                   << "Score:,42.5\n"
                   << "Scenario:,Fixture\n"
                   << "Hash:,fixture-hash\n"
                   << "Game Version:,3.0.2\n"
                   << "Challenge Start:,11:59:00.000\n";
        }

        EXPECT_FALSE(parser.parseFile(path));
        std::filesystem::remove(path);
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
