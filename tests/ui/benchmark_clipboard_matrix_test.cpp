#include <gtest/gtest.h>

#include <QString>
#include <vector>

#include "presentation/benchmark_clipboard_matrix.h"

using namespace ksv::presentation;

namespace {
    using Rows = std::vector<std::vector<QString>>;

    Rows decode(const QString &text) { return BenchmarkClipboardMatrix::parse(text).rows; }
}

TEST(BenchmarkClipboardMatrix, QuotedBlanksAndTerminalRecord) {
    const Rows expected{{"A", "", "C", ""}, {""}, {"B", "0", ""}};
    EXPECT_EQ(decode("A\t\tC\t\r\n\r\nB\t0\t\r\n"), expected);
    EXPECT_EQ(decode("A\t\tC\t\n\nB\t0\t\n"), expected);
    EXPECT_EQ(decode("A\t\tC\t\n\nB\t0\t"), expected);

    // Quoted cells may hold delimiters; doubled quotes unescape.
    EXPECT_EQ(decode("\"a\tb\"\t\"line1\r\nline2\"\t\"say \"\"hi\"\"\"\n"),
              (Rows{{"a\tb", "line1\r\nline2", "say \"hi\""}}));
    // A quote inside an unquoted cell is literal text.
    EXPECT_EQ(decode("5\"\tx"), (Rows{{"5\"", "x"}}));
    // Formula- and number-looking text stays text.
    EXPECT_EQ(decode("=SUM(A1:A2)\t1,234.5\t 7 "), (Rows{{"=SUM(A1:A2)", "1,234.5", " 7 "}}));
    // Only one terminal record is routine; a second line break is an intentional blank row.
    EXPECT_EQ(decode("A\n\n"), (Rows{{"A"}, {""}}));
    EXPECT_EQ(decode("A\t"), (Rows{{"A", ""}}));
    // Ragged rows keep their own lengths.
    const auto ragged = BenchmarkClipboardMatrix::parse("a\tb\tc\nd\n");
    EXPECT_EQ(ragged.rows, (Rows{{"a", "b", "c"}, {"d"}}));
    EXPECT_EQ(ragged.width(), 3);
}

// Spreadsheets copy a cell that merely starts with a quote verbatim, so such a quote must never
// swallow later cells or reject the block.
TEST(BenchmarkClipboardMatrix, StrayQuotesStayLiteral) {
    EXPECT_EQ(decode("\"unterminated\tnext\nrow"), (Rows{{"\"unterminated", "next"}, {"row"}}));
    EXPECT_EQ(decode("\"12\t1\n\"13\"\t2\n15\"\t3\n"),
              (Rows{{"\"12", "1"}, {"\"13\"", "2"}, {"15\"", "3"}}));
    EXPECT_EQ(decode("\"a\"b\tc"), (Rows{{"\"a\"b", "c"}}));

    const auto empty = BenchmarkClipboardMatrix::parse(QString());
    EXPECT_TRUE(empty.rows.empty());
    EXPECT_EQ(empty.width(), 0);
}
