#include <gtest/gtest.h>

#include <QLocale>
#include <QString>
#include <bit>
#include <cstdint>
#include <limits>
#include <vector>

#include "presentation/benchmark_cell_input.h"

using namespace ksv::presentation;

namespace {
    const QLocale enGB{QLocale::English, QLocale::UnitedKingdom};
    const QLocale deDE{QLocale::German, QLocale::Germany};

    // Bitwise, so signed zero and the exact subnormal survive the round trip.
    bool identical(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }

    void expectFinite(const QString &text, const QLocale &locale, double expected) {
        const auto parsed = BenchmarkCellInput::parse(text, locale);
        EXPECT_EQ(parsed.outcome, CellParseOutcome::Finite) << text.toStdString();
        EXPECT_DOUBLE_EQ(parsed.value, expected) << text.toStdString();
    }

    void expectOutcome(const QString &text, const QLocale &locale, CellParseOutcome expected) {
        EXPECT_EQ(BenchmarkCellInput::parse(text, locale).outcome, expected)
            << text.toStdString() << " in " << locale.name().toStdString();
    }
}

TEST(BenchmarkTableInput, GroupedScientificAndPrecision) {
    expectFinite("1,234.5", enGB, 1234.5);
    expectFinite("1234.5", enGB, 1234.5);
    expectFinite("  1.5e3 ", enGB, 1500.0);
    expectFinite("-2.5E-3", enGB, -0.0025);
    expectFinite("0", enGB, 0.0);
    expectFinite("1.234,5", deDE, 1234.5);
    expectFinite("1234,5", deDE, 1234.5);
    expectFinite("1,5e3", deDE, 1500.0);
    expectOutcome("   ", enGB, CellParseOutcome::Missing);
    expectOutcome("", deDE, CellParseOutcome::Missing);

    const std::vector<double> values{1234.5678901234567, 1.7976931348623157e308, 4.9406564584124654e-324,
                                     -0.0, 1e-7, 123456789012.25, 0.1, 1e21};
    for (const auto &locale: {enGB, deDE}) {
        for (const double value: values) {
            const auto display = BenchmarkCellInput::displayText(value, locale);
            const auto edit = BenchmarkCellInput::editText(value, locale);
            const auto fromDisplay = BenchmarkCellInput::parse(display, locale);
            const auto fromEdit = BenchmarkCellInput::parse(edit, locale);
            EXPECT_EQ(fromDisplay.outcome, CellParseOutcome::Finite) << display.toStdString();
            EXPECT_TRUE(identical(fromDisplay.value, value)) << display.toStdString();
            EXPECT_EQ(fromEdit.outcome, CellParseOutcome::Finite) << edit.toStdString();
            EXPECT_TRUE(identical(fromEdit.value, value)) << edit.toStdString();
            EXPECT_FALSE(edit.contains(locale.groupSeparator())) << edit.toStdString();
        }
    }
    EXPECT_EQ(BenchmarkCellInput::displayText(1234.5, enGB), "1,234.5");
    EXPECT_EQ(BenchmarkCellInput::editText(1234.5, enGB), "1234.5");
    EXPECT_EQ(BenchmarkCellInput::displayText(1234.5, deDE), "1.234,5");
}

TEST(BenchmarkTableInput, MalformedAndNonFiniteRemainExact) {
    for (const auto *text: {"12oops", "=1+2", "1,23,4", "1,2345.0", "12 34", "0x10", "--1"})
        expectOutcome(QString::fromLatin1(text), enGB, CellParseOutcome::Invalid);
    // Another locale's conventions are not guessed.
    expectOutcome("1,234.5", deDE, CellParseOutcome::Invalid);
    for (const auto *text: {"NaN", "nan", "infinity", "-Infinity", "inf", "1e9999", "-1e9999"})
        expectOutcome(QString::fromLatin1(text), enGB, CellParseOutcome::NonFinite);
}
