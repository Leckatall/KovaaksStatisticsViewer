//
// Created by Lecka on 30/07/2026.
//

#include "graph_vm.h"


namespace ksv::presentation {
    GraphViewModel::GraphViewModel(QObject *parent) : QAbstractTableModel(parent) {
        m_points = {{0, 1}, {1, 3}, {2, 2}, {3, 5}};
        recomputeBounds();
    }


    QVariant GraphViewModel::data(const QModelIndex &index, int role) const {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};
        if (role != Qt::DisplayRole && role != Qt::EditRole)
            return {};
        const QPointF &p = m_points.at(index.row());
        return index.column() == XColumn ? p.x() : p.y();
    }

    void GraphViewModel::setPoints(QList<QPointF> points) {
        beginResetModel();
        m_points = std::move(points);
        endResetModel();
        recomputeBounds();
    }

    void GraphViewModel::appendPoint(qreal x, qreal y) {
        const int pos = m_points.size();
        beginInsertRows(QModelIndex(), pos, pos);
        m_points.append({x, y});
        endInsertRows();
        recomputeBounds();
    }

    namespace {
        double niceStep(double rawStep) {
            // "Nice number" scaling: 1, 2, 5 × 10^n
            if (rawStep <= 0.0) return 1.0;
            const double exp10 = std::pow(10.0, std::floor(std::log10(rawStep)));
            const double f = rawStep / exp10;
            double nf = 1.0;
            if (f <= 1.0) nf = 1.0;
            else if (f <= 2.0) nf = 2.0;
            else if (f <= 5.0) nf = 5.0;
            else nf = 10.0;
            return nf * exp10;
        }
    }

    void GraphViewModel::recomputeBounds() {
        double newXMin, newXMax, newYMin, newYMax;

        if (m_points.isEmpty()) {
            // sensible defaults
            newXMin = 0.0;
            newXMax = 1.0;
            newYMin = 0.0;
            newYMax = 1.0;
        } else {
            auto [xItMin, xItMax] = std::minmax_element(
                m_points.cbegin(), m_points.cend(),
                [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });

            auto [yItMin, yItMax] = std::minmax_element(
                m_points.cbegin(), m_points.cend(),
                [](const QPointF &a, const QPointF &b) { return a.y() < b.y(); });

            double xlo = xItMin->x();
            double xhi = xItMax->x();
            double ylo = yItMin->y();
            double yhi = yItMax->y();

            // Handle degenerate ranges (all x same or all y same)
            if (xlo == xhi) {
                xlo -= 0.5;
                xhi += 0.5;
            }
            if (ylo == yhi) {
                ylo -= 0.5;
                yhi += 0.5;
            }

            // Pad by ~5%
            const double xPad = (xhi - xlo) * 0.05;
            const double yPad = (yhi - ylo) * 0.05;
            xlo -= xPad;
            xhi += xPad;
            ylo -= yPad;
            yhi += yPad;

            // Snap to nice steps (target ~8 ticks)
            const double xStep = niceStep((xhi - xlo) / 8.0);
            const double yStep = niceStep((yhi - ylo) / 8.0);

            newXMin = std::floor(xlo / xStep) * xStep;
            newXMax = std::ceil(xhi / xStep) * xStep;
            newYMin = std::floor(ylo / yStep) * yStep;
            newYMax = std::ceil(yhi / yStep) * yStep;
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
}
