#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_FORMAT_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_FORMAT_H

#include <QColor>
#include <QString>
#include <vector>

#include "benchmarks/benchmark_ids.h"

namespace ksv::presentation {
    [[nodiscard]] QColor toQColor(const domain::BenchmarkColor &color);

    // A benchmark-defined colour (e.g. a tier's) as a text colour that stays readable on the app's
    // dark surfaces: opaque, and lightened toward white only as far as needed to reach a WCAG
    // contrast ratio of 4.5:1 against the lightest dark panel. Light colours pass through unchanged.
    [[nodiscard]] QColor readableTextColor(const domain::BenchmarkColor &color);

    // WCAG 2 contrast ratio between two opaque colours (1..21).
    [[nodiscard]] double contrastRatio(const QColor &a, const QColor &b);

    // The lightest dark surface benchmark text is drawn on (the summary/card panels).
    [[nodiscard]] QColor darkPanelColor();

    // Locale-formatted, rounded to two decimals - the shared presentation form for
    // benchmark scores, thresholds and averages.
    [[nodiscard]] QString formatBenchmarkNumber(double value);

    // An average rank (the mean normalized rank: whole numbers are tiers, 0 Unranked and k the
    // k-th tier in `tierNames`, the fraction progress toward the next) in words:
    // "Bronze · 60% to Silver", "Unranked · 25% to Iron", and at the top "Gold · top tier reached".
    // The percentage is floored, so it never reads 100% before the next tier is reached. With no
    // tiers there is nothing to name, so it falls back to formatBenchmarkNumber().
    [[nodiscard]] QString describeAverageRank(double rank, const std::vector<QString> &tierNames);
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_FORMAT_H
