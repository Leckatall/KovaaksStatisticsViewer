#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_CELL_INPUT_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_CELL_INPUT_H

#include <QLocale>
#include <QString>
#include <compare>

#include "benchmarks/benchmark_ids.h"

namespace ksv::presentation {
    struct BenchmarkCellKey {
        domain::ScenarioEntryId entryId;
        domain::TierId tierId;
        auto operator<=>(const BenchmarkCellKey &) const = default;
    };

    enum class CellParseOutcome { Missing, Finite, Invalid, NonFinite };

    struct CellParseResult {
        CellParseOutcome outcome = CellParseOutcome::Missing;
        double value = 0.0; // meaningful only for Finite
    };

    // Temporary threshold input that cannot become a typed score. Only Invalid and NonFinite
    // input is retained; its text is kept exactly as the user supplied it.
    struct BenchmarkCellRecord {
        QString rawText;
        CellParseOutcome outcome = CellParseOutcome::Invalid;
        friend bool operator==(const BenchmarkCellRecord &, const BenchmarkCellRecord &) = default;
    };

    class BenchmarkCellInput {
    public:
        static CellParseResult parse(const QString &text, const QLocale &locale);
        // Grouped, for normal display; never rounds away precision.
        static QString displayText(double score, const QLocale &locale);
        // Ungrouped, for the actively edited cell; parses back to the identical double.
        static QString editText(double score, const QLocale &locale);
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_CELL_INPUT_H
