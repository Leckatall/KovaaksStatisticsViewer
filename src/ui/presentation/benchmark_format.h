#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_FORMAT_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_FORMAT_H

#include <QColor>
#include <QString>

#include "benchmarks/benchmark_ids.h"

namespace ksv::presentation {
    [[nodiscard]] QColor toQColor(const domain::BenchmarkColor &color);

    // Locale-formatted, rounded to two decimals - the shared presentation form for
    // benchmark scores, thresholds and averages.
    [[nodiscard]] QString formatBenchmarkNumber(double value);
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_FORMAT_H
