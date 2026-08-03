//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"

#include <qurl.h>
#include <utility>


namespace ksv::presentation {
    GraphViewModel::GraphViewModel(std::shared_ptr<application::IGraphUseCase> graphUseCase,
                                   QObject *parent) : QAbstractTableModel(parent),
                                                      m_graphUseCase(std::move(graphUseCase)) {
        recomputeBounds();
    }

    QVariant GraphViewModel::data(const QModelIndex &index, int role) const {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};

        const auto &row = m_data.at(index.row());
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            switch (static_cast<Column>(index.column())) {
                case Time:
                    return row[Time];
                case Score:
                    return row[Score];
                case Accuracy:
                    return row[Accuracy];
                case ColumnCount:
                    break;
            }
        }
        return {};
    }

    void GraphViewModel::setData(QList<QMap<Column, qreal>> data) {
        beginResetModel();
        m_data = std::move(data);
        endResetModel();
        recomputeBounds();
    }

    void GraphViewModel::setColumn(Column column, QList<qreal> col_data) {
        beginResetModel();
        m_data.resize(col_data.size());
        for (auto &row: m_data) row[column] = col_data.takeFirst();
        endResetModel();
        recomputeBounds();
        emit boundsChanged();
    }

    void GraphViewModel::recomputeBounds() {
        double newXMin, newXMax, newYMin, newYMax;

        newXMin = 0.0;
        newXMax = 60.0;
        newYMin = 0.0;
        newYMax = 5.0;

        if (m_data.isEmpty()) {
            // sensible defaults
            newXMin = 0.0;
            newXMax = 60.0;
            newYMin = 0.0;
            newYMax = 1.0;
        } else {
            // TODO: Re-enable dynamic bounds
            // auto [xItMin, xItMax] = std::minmax_element(
            //     m_data.cbegin(), m_data.cend(),
            //     [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
            //
            // auto [yItMin, yItMax] = std::minmax_element(
            //     m_data.cbegin(), m_data.cend(),
            //     [](const QPointF &a, const QPointF &b) { return a.y() < b.y(); });
            //
            // double xlo = xItMin->x();
            // double xhi = xItMax->x();
            //
            // double ylo = yItMin->y();
            // double yhi = yItMax->y();
            //
            // // Handle degenerate ranges (all x same or all y same)
            // if (xlo == xhi) {
            //     xlo -= 0.5;
            //     xhi += 0.5;
            // }
            // if (ylo == yhi) {
            //     ylo -= 0.5;
            //     yhi += 0.5;
            // }
            //
            // // Pad by ~5%
            // const double xPad = (xhi - xlo) * 0.05;
            // const double yPad = (yhi - ylo) * 0.05;
            //
            // floor(xlo) == 0.0F ? newXMin = 0.0F : newXMin = xlo - xPad;
            // ceil(xhi) == 60.0F ? newXMax = 60.0F : newXMax = xhi + xPad;
            // ylo == 0.0F ? newYMin = ylo : newYMin = ylo - yPad;
            // yhi == 1.0F ? newYMax = yhi : newYMax = yhi + yPad;
        }

        // Only notify if something actually changed
        const bool changed =
                !qFuzzyCompare(1.0 + m_xMin, 1.0 + newXMin) ||
                !qFuzzyCompare(1.0 + m_xMax, 1.0 + newXMax) ||
                !qFuzzyCompare(1.0 + m_yMin, 1.0 + newYMin) ||
                !qFuzzyCompare(1.0 + m_yMax, 1.0 + newYMax);

        if (!changed) return;

        m_xMin = newXMin;
        m_xMax = newXMax;
        m_yMin = newYMin;
        m_yMax = newYMax;

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
        assert(times.size() == scores.size());
        assert(times.size() == accuracies.size());

        const QList<qreal> q_times(times.begin(), times.end());
        setColumn(Time, q_times);
        const QList<qreal> q_scores(scores.begin(), scores.end());
        setColumn(Score, q_scores);
        const QList<qreal> q_accuracies(accuracies.begin(), accuracies.end());
        setColumn(Accuracy, q_accuracies);
    }
}
