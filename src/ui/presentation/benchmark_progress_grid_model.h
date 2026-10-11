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
    // The fill of each equal-width chunk of a segmented tier bar, one chunk per tier in ladder
    // order. Chunk k covers scores from tier k-1's threshold to tier k's (chunk 0 starts at 0), and
    // fills clamp((score - t[k-1]) / (t[k] - t[k-1]), 0, 1): the same in-tier fraction the domain's
    // normalized rank uses, so the filled length in chunk units equals ScenarioProjection's
    // normalizedRank whenever the ladder is valid. A chunk whose bounding thresholds are missing,
    // duplicated, negative, non-finite or not increasing stays empty, and so does every chunk
    // without a score.
    [[nodiscard]] std::vector<double> scenarioChunkFills(const std::vector<domain::Tier> &tiers,
                                                         const std::vector<domain::Threshold> &thresholds,
                                                         std::optional<double> score);

    // Per tier chunk, whether a bound it needs is unusable: its own threshold, or the previous
    // tier's (chunk 0 starts at 0), is missing, duplicated, negative or non-finite, or the two
    // are not increasing. Such a chunk can never fill; the bar draws it hatched.
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
    // tier count (bar end): the domain's normalized tier value. No position without a score or
    // when the ladder is incomplete or not increasing, so a marker is never drawn at a made-up
    // place.
    [[nodiscard]] std::optional<double> tierScalePosition(const std::vector<domain::Tier> &tiers,
                                                          const std::vector<domain::Threshold> &thresholds,
                                                          std::optional<double> score);
}

#endif // KOVAAKSSTATSVIEWER_BENCHMARK_PROGRESS_GRID_MODEL_H
