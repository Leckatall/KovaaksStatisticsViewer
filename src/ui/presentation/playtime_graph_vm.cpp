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
        map[QString::number(Date)] = QPointF(m_xBounds.first, m_xBounds.second);
        map[QString::number(Playtime)] = QPointF(m_yBounds.first, m_yBounds.second);
        return map;
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

        // X axis spans the first..last day; a single point gets a symmetric
        // pad so the axis isn't zero-width. Y starts at 0 with ~5% headroom.
        if (m_points.isEmpty()) {
            m_xBounds = {0.0, 1.0};
            m_yBounds = {0.0, 1.0};
        } else {
            const qreal xlo = m_points.first().x();
            const qreal xhi = m_points.last().x();
            m_xBounds = xlo == xhi ? std::pair{xlo - 0.5, xhi + 0.5} : std::pair{xlo, xhi};

            qreal ymax = 0.0;
            for (const auto &p: m_points) ymax = std::max(ymax, p.y());
            m_yBounds = {0.0, ymax > 0.0 ? ymax * 1.05 : 1.0};
        }

        emit pointCountChanged();
        emit boundsChanged();
    }
}
