//
// Created by Lecka on 08/08/2026.
//

#include "playtime_graph_vm.h"

#include <utility>

#include "date_axis.h"

namespace ksv::presentation {
    namespace {
        const QColor kPlaytimeColor("#4DD0E1");
    }

    PlaytimeGraphViewModel::PlaytimeGraphViewModel(std::shared_ptr<application::IPlaytimeGraphUseCase> useCase,
                                                   QObject *parent)
        : GraphViewModelBase(parent), m_useCase(std::move(useCase)), m_series(new SeriesModel(this)) {
        refresh();
    }

    void PlaytimeGraphViewModel::refresh() {
        const auto rollingPlaytime = m_useCase->get_rolling_playtime(kWindowDays);

        QList<QPointF> rawSecondsPoints;
        rawSecondsPoints.reserve(int(rollingPlaytime.size()));
        for (const auto &[epoch_day, avg_seconds]: rollingPlaytime) {
            const auto epochMs = static_cast<qreal>(epochDayToUtcMs(epoch_day));
            rawSecondsPoints.append(QPointF(epochMs, avg_seconds));
        }

        if (rawSecondsPoints.isEmpty()) {
            m_xAxis = utcDayAxis(0.0, 0.0, false);
        } else {
            m_xAxis = utcDayAxis(rawSecondsPoints.first().x(), rawSecondsPoints.last().x(), true);
        }

        m_series->setId(QString::number(Playtime));
        m_series->setName("Playtime (3-day avg)");
        m_series->setColor(kPlaytimeColor);
        m_series->setColumn(Playtime);
        m_series->transform = ValueTransform::secondsToMinutes(); // Plots raw seconds, presents minutes
        m_series->yAxisOptions = {.baseline = AxisModel::Baseline::Zero};
        m_series->setData(rawSecondsPoints);
        emit dataUpdated();
        emit boundsChanged();
    }
}
