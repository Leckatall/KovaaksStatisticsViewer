//
// Created by Lecka on 08/08/2026.
//

#include "playtime_graph_vm.h"

#include <QDate>
#include <algorithm>
#include <utility>

namespace ksv::presentation {
    namespace {
        const QColor kPlaytimeColor("#4DD0E1");
    }

    PlaytimeGraphViewModel::PlaytimeGraphViewModel(std::shared_ptr<application::IPlaytimeGraphUseCase> useCase,
                                                   QObject *parent)
        : GraphViewModelBase(parent), m_useCase(std::move(useCase)) {
        refresh();
    }

    QVariantMap PlaytimeGraphViewModel::axisBounds() const {
        QVariantMap map;
        map[QString::number(Date)] = QPointF(m_xAxis.min(), m_xAxis.max());
        map[QString::number(Playtime)] = QPointF(m_yAxis.min(), m_yAxis.max());
        return map;
    }

    QList<qreal> PlaytimeGraphViewModel::axisTicks(const int column) const {
        if (column == Date) return m_xAxis.ticks();
        if (column == Playtime) return m_yAxis.ticks();
        return {};
    }

    QList<QPointF> PlaytimeGraphViewModel::seriesPoints(const int column) const {
        if (column != Playtime) return {};
        return m_points;
    }

    QString PlaytimeGraphViewModel::columnName(const int column) const {
        switch (column) {
            case Date: return "Date";
            case Playtime: return "Playtime (3-day avg)";
            default: return {};
        }
    }

    QColor PlaytimeGraphViewModel::columnColor(const int column) const {
        if (column == Playtime) return kPlaytimeColor;
        return {};
    }

    QString PlaytimeGraphViewModel::formatXTick(const qreal value) const {
        // value is a day count since the Unix epoch (see PlaytimeGraphUseCase).
        const QDate date = QDate(1970, 1, 1).addDays(qint64(std::llround(value)));
        return date.toString("MMM d");
    }

    void PlaytimeGraphViewModel::refresh() {
        const auto series = m_useCase->get_rolling_playtime(kWindowDays);

        m_points.clear();
        m_points.reserve(int(series.size()));
        for (const auto &[epoch_day, avg_seconds]: series) {
            m_points.append(QPointF(qreal(epoch_day), avg_seconds / 60.0));
        }

        // X spans the first..last day, rounded to nice whole-day boundaries;
        // the Date axis is integral so ticks never fall on fractional days.
        // Y starts at 0 and rounds up to a nice value.
        if (m_points.isEmpty()) {
            m_xAxis = AxisModel::forRange(0.0, 1.0, {AxisModel::Baseline::HugData, /*integral=*/true});
            m_yAxis = AxisModel::forRange(0.0, 1.0, {AxisModel::Baseline::Zero});
        } else {
            const qreal xlo = m_points.first().x();
            const qreal xhi = m_points.last().x();
            m_xAxis = AxisModel::forRange(xlo, xhi, {AxisModel::Baseline::HugData, /*integral=*/true});

            qreal ymax = 0.0;
            for (const auto &p: m_points) ymax = std::max(ymax, p.y());
            m_yAxis = AxisModel::forRange(0.0, ymax, {AxisModel::Baseline::Zero});
        }

        emit pointCountChanged();
        emit boundsChanged();
    }
}
