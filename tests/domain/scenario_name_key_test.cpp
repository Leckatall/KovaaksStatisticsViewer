#include <gtest/gtest.h>

#include "benchmarks/benchmark_resolution.h"

using ksv::domain::scenarioNameKey;

TEST(ScenarioNameKey, IgnoresCase) {
    EXPECT_EQ(scenarioNameKey("1wall6targets TE"), scenarioNameKey("1Wall6Targets te"));
}

// Spreadsheet cells routinely carry trailing spaces, tabs and non-breaking spaces.
TEST(ScenarioNameKey, IgnoresSurroundingWhitespaceIncludingNonBreakingSpaces) {
    EXPECT_EQ(scenarioNameKey("  Air Angelic 4\t \r\n"), scenarioNameKey("Air Angelic 4"));
    EXPECT_EQ(scenarioNameKey("\xC2\xA0" "Air Angelic 4" "\xC2\xA0"), scenarioNameKey("Air Angelic 4"));
}

TEST(ScenarioNameKey, KeepsInnerWhitespaceSignificant) {
    EXPECT_NE(scenarioNameKey("Air  Angelic"), scenarioNameKey("Air Angelic"));
    EXPECT_NE(scenarioNameKey("AirAngelic"), scenarioNameKey("Air Angelic"));
}

TEST(ScenarioNameKey, WhitespaceOnlyNameFoldsToEmpty) {
    EXPECT_EQ(scenarioNameKey(" \t\xC2\xA0 "), "");
}

TEST(ScenarioNameKey, LeavesNonAsciiLettersIntact) {
    EXPECT_EQ(scenarioNameKey("Tile Frenzy \xC3\x89"), "tile frenzy \xC3\x89");
}
