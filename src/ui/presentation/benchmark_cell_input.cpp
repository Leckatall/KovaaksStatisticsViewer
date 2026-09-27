#include "benchmark_cell_input.h"

#include <QRegularExpression>
#include <cmath>

namespace ksv::presentation {
    namespace {
        bool spellsNonFinite(const QString &text) {
            static const QRegularExpression pattern(QStringLiteral("^[+-]?(nan|inf|infinity|∞)$"),
                                                    QRegularExpression::CaseInsensitiveOption);
            return pattern.match(text).hasMatch();
        }

        // QLocale drops the sign of negative zero, which would not round-trip.
        QString formatted(double score, const QLocale &locale) {
            const QString text = locale.toString(score, 'g', QLocale::FloatingPointShortest);
            return score == 0.0 && std::signbit(score) ? locale.negativeSign() + text : text;
        }
    }

    CellParseResult BenchmarkCellInput::parse(const QString &text, const QLocale &locale) {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty()) return {CellParseOutcome::Missing};
        if (spellsNonFinite(trimmed)) return {CellParseOutcome::NonFinite};
        bool ok = false;
        const double value = locale.toDouble(trimmed, &ok);
        if (ok) return std::isfinite(value) ? CellParseResult{CellParseOutcome::Finite, value}
                                            : CellParseResult{CellParseOutcome::NonFinite};
        // QLocale::toDouble reports an out-of-range literal such as 1e9999 as a failed conversion
        // returning infinity; that is a non-finite number, not non-numeric text.
        return {std::isinf(value) ? CellParseOutcome::NonFinite : CellParseOutcome::Invalid};
    }

    QString BenchmarkCellInput::displayText(double score, const QLocale &locale) {
        QLocale grouped = locale;
        grouped.setNumberOptions(grouped.numberOptions() & ~QLocale::OmitGroupSeparator);
        return formatted(score, grouped);
    }

    QString BenchmarkCellInput::editText(double score, const QLocale &locale) {
        QLocale ungrouped = locale;
        ungrouped.setNumberOptions(ungrouped.numberOptions() | QLocale::OmitGroupSeparator);
        return formatted(score, ungrouped);
    }
}
