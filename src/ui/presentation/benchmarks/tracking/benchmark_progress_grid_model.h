#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_PROGRESS_GRID_MODEL_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_PROGRESS_GRID_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <qqmlintegration.h>

#include <optional>
#include <span>
#include <vector>

#include "benchmarks/benchmark.h"
#include "presentation/benchmarks/tracking/progress_grid_content.h"

namespace ksv::presentation {
    // A segmented tier bar has one equal-width chunk per tier in ladder order, and chunk k is the
    // progress toward tier k: from tier k-1's threshold (0 for the first tier) up to tier k's, so
    // a chunk is full exactly when its tier is reached and the bar is full once the top tier is.
    // The filled length in chunk units is the domain's normalized tier value (6.37 = six full
    // chunks and 37% of the seventh), the scale the rank graph and the average rank use.

    // A rank on the domain's normalized scale (Benchmark::averageRank: k = tier k attained,
    // fractions part-way to the next) as tier bar chunk fills: chunk k (0-based) fills
    // clamp(rank - k, 0, 1), so exactly k ends at chunk k-1's right edge.
    [[nodiscard]] std::vector<double> rankChunkFills(double rank, std::size_t tierCount);

    // The fill of each chunk of a scenario's tier bar. Chunk k fills
    // clamp((score - t[k-1]) / (t[k] - t[k-1]), 0, 1), with t[-1] = 0: the same in-tier fraction
    // the domain's normalized rank uses, so the filled length equals it whenever the ladder is
    // valid. A chunk whose bounding thresholds are missing, duplicated, negative, non-finite or not
    // increasing stays empty, and so does every chunk without a score.
    [[nodiscard]] std::vector<double> scenarioChunkFills(std::span<const domain::Tier> tiers,
                                                         const domain::ScenarioBenchmarkEvaluation &scenario,
                                                         std::optional<double> score);

    // Per tier chunk, whether a bound it needs is unusable: its own threshold, or the previous
    // tier's (chunk 0 starts at 0), is missing, duplicated, negative or non-finite, or out of order
    // with a neighbouring threshold (not above the one before it, or not below the one after it;
    // either of the two may be the wrong one). Such a chunk never fills; the bar draws it hatched.
    [[nodiscard]] std::vector<bool> missingThresholdChunks(std::span<const domain::Tier> tiers,
                                                           const domain::ScenarioBenchmarkEvaluation &scenario);

    // Row order follows the benchmark tree: Uncategorized first, then each category's direct
    // scenarios and its subcategories. Empty groups have no spanning cell.
    // Match states appear only when abnormal. BenchmarkTrackingViewModel owns the grid and resets it once per revision.
    class BenchmarkProgressGridModel final : public QObject {
        Q_OBJECT
        // [{id, name, color, barColor}]: name and readable text colour for the header, and the
        // tier's own opaque colour for bar fills.
        Q_PROPERTY(QVariantList tiers READ tiers NOTIFY changed)
        // One map per scenario row; see the .cpp for its keys.
        Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
        // [{column: "category"|"subcategory", id, name, color, start, span}]: the group cells,
        // each spanning `span` rows from row `start`.
        Q_PROPERTY(QVariantList groups READ groups NOTIFY changed)
        Q_PROPERTY(bool hasSubcategories READ hasSubcategories NOTIFY changed)
        // Column headings, keyed category, subcategory, scenario, progress, pbDate, plays, next.
        Q_PROPERTY(QVariantMap headers READ headers CONSTANT)

    public:
        explicit BenchmarkProgressGridModel(QObject *parent = nullptr);

        [[nodiscard]] QVariantList tiers() const { return m_tiers; }
        [[nodiscard]] QVariantList rows() const { return m_rows; }
        [[nodiscard]] QVariantList groups() const { return m_groups; }
        [[nodiscard]] bool hasSubcategories() const { return m_hasSubcategories; }

        [[nodiscard]] QVariantMap headers() const;

        // The typed form the three lists are derived from.
        [[nodiscard]] const ProgressGridContent &content() const { return m_content; }

        // Null empties the grid.
        void reset(const domain::Benchmark *benchmark);

    signals:
        void changed();

    private:
        ProgressGridContent m_content;
        QVariantList m_tiers;
        QVariantList m_rows;
        QVariantList m_groups;
        bool m_hasSubcategories = false;
    };
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_PROGRESS_GRID_MODEL_H
