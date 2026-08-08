//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"

#include <qurl.h>
#include <algorithm>
#include <utility>
#include <QVector>


namespace ksv::presentation {
    namespace {
        struct ColumnMeta {
            const char *name;
            QColor color;
        };

        // Indexed by GraphViewModel::Column. Time has no line of its own (it's
        // the X axis) so its entry is unused but kept for array alignment.
        const std::array<ColumnMeta, GraphViewModel::ColumnCount> kColumnMeta{{
            {"Time", QColor()},
            {"Score", QColor("#009600")},
            {"Accuracy", QColor("cyan")},
            {"Shots", QColor("orange")},
            {"Kills", QColor("red")},
            {"Dmg", QColor("yellow")},
        }};
    }

    GraphViewModel::GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase,
                                   QObject *parent) : GraphViewModelBase(parent),
                                                      m_graphUseCase(std::move(graphUseCase)) {
        recomputeBounds();
    }

    void GraphViewModel::setData(QList<QMap<Column, qreal>> data) {
        m_data = std::move(data);
        emit pointCountChanged();
        recomputeBounds();
    }

    QVariantList GraphViewModel::plottableColumns() const {
        QVariantList columns;
        for (int c = Score; c < ColumnCount; ++c) columns.append(c);
        return columns;
    }

    QVariantMap GraphViewModel::axisBounds() const {
        QVariantMap map;
        for (int c = 0; c < ColumnCount; ++c) {
            map[QString::number(c)] = QPointF(m_bounds[c].first, m_bounds[c].second);
        }
        return map;
    }

    QString GraphViewModel::columnName(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return QString::fromLatin1(kColumnMeta[column].name);
    }

    QColor GraphViewModel::columnColor(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return kColumnMeta[column].color;
    }

    QList<QPointF> GraphViewModel::seriesPoints(const int column) const {
        if (column < 0 || column >= ColumnCount) return {};
        const auto col = static_cast<Column>(column);
        QList<QPointF> points;
        points.reserve(m_data.size());
        for (const auto &row: m_data) points.append(QPointF(row[Time], row[col]));
        return points;
    }

    namespace {
        // Real min/max of `column` across `data`, no padding applied.
        std::pair<qreal, qreal> rawColumnRange(const QList<QMap<GraphViewModel::Column, qreal>> &data,
                                               const GraphViewModel::Column column) {
            qreal lo = data.front()[column];
            qreal hi = lo;
            for (const auto &row: data) {
                lo = std::min(lo, row[column]);
                hi = std::max(hi, row[column]);
            }
            return {lo, hi};
        }

        // Pads [lo, hi] by ~5% on both ends. If every value is identical, pads by a
        // fixed amount instead so the axis isn't zero-width.
        std::pair<qreal, qreal> padded(qreal lo, qreal hi) {
            if (lo == hi) return {lo - 0.5, hi + 0.5};
            const qreal pad = (hi - lo) * 0.05;
            return {lo - pad, hi + pad};
        }

        // Real .perf data ticks arrive as per-tick deltas (see
        // ScenarioCompletionData's comment in scenario_perf.h), not
        // cumulative totals, and can arrive at irregular intervals -
        // including, near the end of some runs, two ticks only ~0.02s apart
        // instead of the usual ~1s spacing. Rounding every row's Time to the
        // nearest whole second and merging rows that land on the same second
        // fixes both: the plot gets one point per second, and a merged
        // near-duplicate tick no longer shows up as an anomalously small
        // delta next to its neighbor - its value is summed into the second
        // it belongs to instead of silently overwriting or diluting it.
        //
        // Score/Shots/Kills/Dmg are directly additive deltas, so summing raw
        // values is correct. Accuracy is a ratio (hits/shots), so summing or
        // averaging the ratio itself would be wrong; instead this recovers
        // hits_i = Accuracy_i * Shots_i (exact, including when Shots_i is 0,
        // since Accuracy_i is defined as 0 in that case too), sums hits and
        // shots separately per bucket, and divides at the end.
        //
        // A second with no raw row defaults to all-zero - these are deltas,
        // so "no data" means "nothing happened", not "unknown".
        QList<QMap<GraphViewModel::Column, qreal>> resampleToWholeSeconds(
            const QList<QMap<GraphViewModel::Column, qreal>> &rawRows) {
            using Column = GraphViewModel::Column;
            if (rawRows.isEmpty()) return rawRows;

            int maxSecond = 0;
            for (const auto &row: rawRows) maxSecond = std::max(maxSecond, qRound(row[Column::Time]));

            QVector<qreal> score(maxSecond + 1, 0.0), shots(maxSecond + 1, 0.0), hits(maxSecond + 1, 0.0),
                    kills(maxSecond + 1, 0.0), dmg(maxSecond + 1, 0.0);

            for (const auto &row: rawRows) {
                const int bucket = std::clamp(qRound(row[Column::Time]), 0, maxSecond);
                const qreal rowShots = row[Column::Shots];
                score[bucket] += row[Column::Score];
                shots[bucket] += rowShots;
                hits[bucket] += row[Column::Accuracy] * rowShots;
                kills[bucket] += row[Column::Kills];
                dmg[bucket] += row[Column::Dmg];
            }

            QList<QMap<Column, qreal>> result(maxSecond + 1);
            for (int s = 0; s <= maxSecond; ++s) {
                result[s][Column::Time] = qreal(s);
                result[s][Column::Score] = score[s];
                result[s][Column::Shots] = shots[s];
                result[s][Column::Accuracy] = shots[s] > 0.0 ? hits[s] / shots[s] : 0.0;
                result[s][Column::Kills] = kills[s];
                result[s][Column::Dmg] = dmg[s];
            }
            return result;
        }
    }

    void GraphViewModel::recomputeBounds() {
        std::array<std::pair<qreal, qreal>, ColumnCount> newBounds{};
        newBounds[Time] = {0.0, 60.0};
        for (int c = Score; c < ColumnCount; ++c) newBounds[c] = {0.0, 1.0};

        if (!m_data.isEmpty()) {
            // Time never goes negative and always starts at 0, so its min is
            // pinned rather than padded below; only its max gets padding.
            const auto [xlo, xhi] = rawColumnRange(m_data, Time);
            const qreal newXMax = m_data.size() == 1 || xlo == xhi ? xhi + 0.5 : xhi + (xhi - xlo) * 0.05;
            newBounds[Time] = {0.0, newXMax};

            for (int c = Score; c < ColumnCount; ++c) {
                const auto [lo, hi] = rawColumnRange(m_data, static_cast<Column>(c));
                newBounds[c] = padded(lo, hi);
            }
        }

        // Only notify if something actually changed
        bool changed = false;
        for (int c = 0; c < ColumnCount; ++c) {
            if (!qFuzzyCompare(1.0 + m_bounds[c].first, 1.0 + newBounds[c].first) ||
                !qFuzzyCompare(1.0 + m_bounds[c].second, 1.0 + newBounds[c].second)) {
                changed = true;
                break;
            }
        }

        if (!changed) return;

        m_bounds = newBounds;

        emit boundsChanged();
    }

    void GraphViewModel::fetchData(const QString &scenario_id) {
        if (!scenario_id.isEmpty()) m_graphUseCase->load_perf(QUrl(scenario_id).toLocalFile().toStdString());

        const QString newTitle = QString::fromStdString(m_graphUseCase->get_run_label());
        if (newTitle != m_scenarioTitle) {
            m_scenarioTitle = newTitle;
            emit scenarioTitleChanged();
        }

        const std::vector<float> times = m_graphUseCase->get_times();
        assert(!times.empty());
        const std::vector<float> scores = m_graphUseCase->get_scores();
        assert(!scores.empty());
        const std::vector<float> accuracies = m_graphUseCase->get_accuracies();
        assert(!accuracies.empty());
        const std::vector<int> shots = m_graphUseCase->get_shots();
        const std::vector<int> kills = m_graphUseCase->get_kills();
        const std::vector<float> dmg = m_graphUseCase->get_dmg();
        assert(times.size() == scores.size());
        assert(times.size() == accuracies.size());

        QList<QMap<Column, qreal>> rows(int(times.size()));
        for (int i = 0; i < int(times.size()); ++i) {
            rows[i][Time] = times[i];
            rows[i][Score] = scores[i];
            rows[i][Accuracy] = accuracies[i];
            rows[i][Shots] = i < int(shots.size()) ? shots[i] : 0.0;
            rows[i][Kills] = i < int(kills.size()) ? kills[i] : 0.0;
            rows[i][Dmg] = i < int(dmg.size()) ? dmg[i] : 0.0;
        }
        setData(resampleToWholeSeconds(rows));
    }
}
