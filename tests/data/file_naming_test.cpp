#include <gtest/gtest.h>

#include <set>
#include <string>

#include "files/file_naming.h"

using namespace ksv::data::files;

namespace {
    // U+1F600, one code point stored as a UTF-16 surrogate pair.
    const std::string kAstral = "\xF0\x9F\x98\x80";

    std::string repeat(const std::string &text, std::size_t count) {
        std::string out;
        for (std::size_t i = 0; i < count; ++i) out += text;
        return out;
    }

    TEST(FileNamingTest, Utf16LengthCountsSurrogatePairsAsTwo) {
        EXPECT_EQ(utf16Length("abc"), 3U);
        EXPECT_EQ(utf16Length("\xC3\xA4"), 1U);
        EXPECT_EQ(utf16Length(kAstral), 2U);
    }

    TEST(FileNamingTest, CheckNameRefusesBlankAndWhitespaceOnlyNames) {
        EXPECT_EQ(checkName("", ".json"), FileNameProblem::Blank);
        EXPECT_EQ(checkName("   ", ".json"), FileNameProblem::Blank);
    }

    TEST(FileNamingTest, CheckNameRefusesEachInvalidCharacterAndControlCharacters) {
        for (const char ch: std::string{"<>:\"/\\|?*"})
            EXPECT_EQ(checkName(std::string{"a"} + ch + "b", ".json"), FileNameProblem::InvalidCharacter) << ch;
        EXPECT_EQ(checkName(std::string{"a\x01" "b"}, ".json"), FileNameProblem::InvalidCharacter);
        EXPECT_EQ(checkName(std::string{"a\x1F" "b"}, ".json"), FileNameProblem::InvalidCharacter);
        EXPECT_EQ(checkName(std::string{"a\0b", 3}, ".json"), FileNameProblem::InvalidCharacter);
    }

    TEST(FileNamingTest, CheckNameCountsTheLengthLimitInUtf16Units) {
        EXPECT_EQ(checkName(repeat("a", 250), ".json"), std::nullopt);
        EXPECT_EQ(checkName(repeat("a", 251), ".json"), FileNameProblem::TooLong);
        EXPECT_EQ(checkName(repeat(kAstral, 125), ".json"), std::nullopt);
        EXPECT_EQ(checkName(repeat(kAstral, 125) + "a", ".json"), FileNameProblem::TooLong);
    }

    TEST(FileNamingTest, CheckNameRefusesTrailingDotsAndSpaces) {
        EXPECT_EQ(checkName("Voltaic.", ".json"), FileNameProblem::TrailingDotOrSpace);
        EXPECT_EQ(checkName("Voltaic ", ".json"), FileNameProblem::TrailingDotOrSpace);
    }

    TEST(FileNamingTest, CheckNameRefusesReservedNamesAloneOrWithAnyExtension) {
        EXPECT_EQ(checkName("CON", ".json"), FileNameProblem::Reserved);
        EXPECT_EQ(checkName("com1", ".json"), FileNameProblem::Reserved);
        EXPECT_EQ(checkName("LPT9.txt", ".json"), FileNameProblem::Reserved);
        EXPECT_EQ(checkName("nul.anything", ".json"), FileNameProblem::Reserved);
        EXPECT_EQ(checkName("CONSOLE", ".json"), std::nullopt);
        EXPECT_EQ(checkName("COM0", ".json"), std::nullopt);
    }

    TEST(FileNamingTest, CheckNameAcceptsALeadingDotAndANameEndingInTheExtension) {
        EXPECT_EQ(checkName(".hidden", ".json"), std::nullopt);
        EXPECT_EQ(checkName("export.json", ".json"), std::nullopt);
    }

    TEST(FileNamingTest, CleanNameRemovesInvalidCharacters) {
        EXPECT_EQ(cleanName("a<b>c:d\"e/f\\g|h?i*j\x01k", ".json", "Benchmark"), "abcdefghijk");
    }

    TEST(FileNamingTest, CleanNameTruncatesWithoutSplittingASurrogatePair) {
        EXPECT_EQ(cleanName(repeat("a", 260), ".json", "Benchmark"), repeat("a", 250));
        // 249 units of 'a' leave one unit, which an astral character cannot fit.
        EXPECT_EQ(cleanName(repeat("a", 249) + kAstral, ".json", "Benchmark"), repeat("a", 249));
    }

    TEST(FileNamingTest, CleanNameTrimsATrailingSpaceExposedByTruncation) {
        EXPECT_EQ(cleanName(repeat("a", 248) + "  bcd", ".json", "Benchmark"), repeat("a", 248));
    }

    TEST(FileNamingTest, CleanNameTrimsTrailingDotsAndSpaces) {
        EXPECT_EQ(cleanName("Voltaic.", ".json", "Benchmark"), "Voltaic");
        EXPECT_EQ(cleanName("Voltaic . .", ".json", "Benchmark"), "Voltaic");
    }

    TEST(FileNamingTest, CleanNameFallsBackForEmptyAndReservedResults) {
        EXPECT_EQ(cleanName("", ".json", "Benchmark"), "Benchmark");
        EXPECT_EQ(cleanName("???", ".json", "Benchmark"), "Benchmark");
        EXPECT_EQ(cleanName(" . ", ".json", "Benchmark"), "Benchmark");
        EXPECT_EQ(cleanName("con", ".json", "Benchmark"), "Benchmark");
    }

    TEST(FileNamingTest, UniqueNameKeepsAnUntakenName) {
        EXPECT_EQ(uniqueName("Bench", ".json", "Benchmark", [](std::string_view) { return false; }), "Bench");
    }

    TEST(FileNamingTest, UniqueNameSuffixesWithTheSmallestFreeNumber) {
        const std::set<std::string, std::less<>> taken{"Bench", "Bench (2)", "Bench (4)"};
        const auto isTaken = [&](std::string_view name) { return taken.contains(name); };
        EXPECT_EQ(uniqueName("Bench", ".json", "Benchmark", isTaken), "Bench (3)");
        EXPECT_EQ(uniqueName("Other", ".json", "Benchmark", isTaken), "Other");
    }

    TEST(FileNamingTest, UniqueNameShortensForTheSuffixAndTrimsWhatTheShorteningExposes) {
        const auto atLimit = repeat("a", 244) + " .bcde";
        const auto isTaken = [&](std::string_view candidate) { return candidate == atLimit; };
        // " (2)" leaves room for 246 units, which end in " ."
        EXPECT_EQ(uniqueName(atLimit, ".json", "Benchmark", isTaken), repeat("a", 244) + " (2)");
    }

    TEST(FileNamingTest, UniqueNameFallsBackWhenShorteningLeavesNothing) {
        const auto name = repeat(" ", 248) + "ab";
        const auto isTaken = [&](std::string_view candidate) { return candidate == name; };
        EXPECT_EQ(uniqueName(name, ".json", "Benchmark", isTaken), "Benchmark (2)");
    }

    TEST(FileNamingTest, StemOfRemovesOneFinalExtensionIgnoringCase) {
        EXPECT_EQ(stemOf("Bench.json", ".json"), "Bench");
        EXPECT_EQ(stemOf("Bench.JSON", ".json"), "Bench");
        EXPECT_EQ(stemOf("a.json.json", ".json"), "a.json");
        EXPECT_EQ(stemOf("notes.txt", ".json"), "notes.txt");
    }
}
