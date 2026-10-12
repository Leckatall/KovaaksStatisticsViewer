#ifndef KOVAAKSSTATSVIEWER_BENCHMARK_PROGRESS_GRID_MODEL_H
#define KOVAAKSSTATSVIEWER_BENCHMARK_PROGRESS_GRID_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <qqmlintegration.h>

#include <optional>
#include <vector>

#include "benchmarks/benchmark.h"
#include "benchmarks/benchmark_projection.h"

namespace ksv::presentation {
    // A segmented tier bar has one equal-width chunk per tier in ladder order, and chunk k is the
    // range in which tier k is the rank attained: from tier k's threshold up to tier k+1's. The
    // furthest filled chunk is therefore always in the attained rank's colour. The top tier's chunk
    // has no upper bound, so it is full once reached; below the first tier the bar is empty.

    // A chunk the score has entered but not moved into never shows a zero fill, so a rank reached
    // exactly on its threshold still shows its colour (the bar draws such a fill a few pixels wide).
    inline constexpr double kEnteredChunkFill = 1e-6;

    // A rank on the domain's normalized scale (BenchmarkProjection::averageRank: k = tier k attained,
    // fractions part-way to the next) as tier bar chunk fills: tier k's chunk (k - 1, 0-based) holds
    // the fraction past k, and the top tier's chunk is full from the top rank on.
    [[nodiscard]] std::vector<double> rankChunkFills(double rank, std::size_t tierCount);

    // The fill of each chunk of a scenario's tier bar. Chunk k fills
    // clamp((score - t[k]) / (t[k+1] - t[k]), 0, 1) once the score reaches t[k], at least
    // kEnteredChunkFill; the last chunk is full once the score reaches the top threshold. A chunk
    // whose bounding thresholds are missing, duplicated, negative, non-finite or not increasing
    // stays empty, and so does every chunk without a score.
    [[nodiscard]] std::vector<double> scenarioChunkFills(const std::vector<domain::Tier> &tiers,
                                                         const std::vector<domain::Threshold> &thresholds,
                                                         std::optional<double> score);

    // Per tier chunk, whether a bound it needs is unusable: its own threshold, or the next tier's
    // (the top tier's chunk has none), is missing, duplicated, negative or non-finite, or out of
    // order with a neighbouring threshold (not above the one before it, or not below the one after
    // it; either of the two may be the wrong one). Such a chunk never fills; the bar draws it hatched.
    [[nodiscard]] std::vector<bool> missingThresholdChunks(const std::vector<domain::Tier> &tiers,
                                                           const std::vector<domain::Threshold> &thresholds);

    // The Benchmarks tab's scenario progress grid, rebuilt from the selected definition joined
    // with its projection: one row per scenario in definition order (Uncategorized first, then each
    // category's direct scenarios and its subcategories), the group cells spanning those rows, and
    // the tier ladder the bars and their header share. Groups without scenarios are left out. A
    // scenario's match state is only reported when it is abnormal. Owned by
    // BenchmarkTrackingViewModel, which resets it once per use-case revision.
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

    public:
        explicit BenchmarkProgressGridModel(QObject *parent = nullptr);

        [[nodiscard]] QVariantList tiers() const { return m_tiers; }
        [[nodiscard]] QVariantList rows() const { return m_rows; }
        [[nodiscard]] QVariantList groups() const { return m_groups; }
        [[nodiscard]] bool hasSubcategories() const { return m_hasSubcategories; }

        // Rebuilds every list for one coherent revision. Null `definition`/`projection` (outside
        // the Ready states) empties the grid.
        void reset(const domain::Benchmark *definition, const domain::BenchmarkProjection *projection);

    signals:
        void changed();

    private:
        QVariantList m_tiers;
        QVariantList m_rows;
        QVariantList m_groups;
        bool m_hasSubcategories = false;
    };

    // Where `score` sits on a tier bar's chunk scale, in chunk units from 0 (bar start) to the
    // tier count (bar end), matching scenarioChunkFills: the first threshold at 0, a score part-way
    // through a tier part-way through its chunk, and the top threshold or above at the bar's end.
    // A score below the first tier sits at 0. No position without a score or when the ladder is
    // incomplete or not increasing, so a marker is never drawn at a made-up place.
    [[nodiscard]] std::optional<double> tierScalePosition(const std::vector<domain::Tier> &tiers,
                                                          const std::vector<domain::Threshold> &thresholds,
                                                          std::optional<double> score);
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_PROGRESS_GRID_MODEL_H
