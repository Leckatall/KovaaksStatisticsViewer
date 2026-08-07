//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"

#include <qurl.h>
#include <utility>


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
                                   QObject *parent) : QAbstractTableModel(parent),
                                                      m_graphUseCase(std::move(graphUseCase)) {
        recomputeBounds();
    }

    QVariant GraphViewModel::data(const QModelIndex &index, int role) const {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};
        if (role != Qt::DisplayRole && role != Qt::EditRole) return {};

        const auto &row = m_data.at(index.row());
        const auto column = static_cast<Column>(index.column());
        if (column < 0 || column >= ColumnCount) return {};
        return row[column];
    }

    void GraphViewModel::setData(QList<QMap<Column, qreal>> data) {
        // QtGraphs' XYModelMapper only reacts to rowsInserted/rowsRemoved/
        // dataChanged, not modelReset — a begin/endResetModel() here leaves
        // its cached series points desynced from the new row count (it can
        // end up with 0 points, crashing spline calculation on the next
        // dataset switch). Use targeted insert/remove/dataChanged instead so
        // the mapper stays in sync.
        const int oldSize = int(m_data.size());
        const int newSize = int(data.size());

        if (newSize < oldSize) beginRemoveRows({}, newSize, oldSize - 1);
        else if (newSize > oldSize) beginInsertRows({}, oldSize, newSize - 1);

        m_data = std::move(data);

        if (newSize < oldSize) endRemoveRows();
        else if (newSize > oldSize) endInsertRows();

        const int commonRows = std::min(oldSize, newSize);
        if (commonRows > 0) emit dataChanged(index(0, 0), index(commonRows - 1, ColumnCount - 1));

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

    QString GraphViewModel::columnName(const Column column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return QString::fromLatin1(kColumnMeta[column].name);
    }

    QColor GraphViewModel::columnColor(const Column column) const {
        if (column < 0 || column >= ColumnCount) return {};
        return kColumnMeta[column].color;
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
        setData(std::move(rows));
    }
}
