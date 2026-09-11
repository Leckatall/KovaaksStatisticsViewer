#include "benchmark_history_vm.h"

#include <QColor>
#include <QString>
#include <cmath>

#include "value_transform.h"

namespace ksv::presentation {
    namespace {
        const QColor kRankColor("#FFB74D");
        const QColor kPlaytimeColor("#4DD0E1");
    }

    BenchmarkHistoryViewModel::BenchmarkHistoryViewModel(const Metric metric, const Axis &sharedXAxis,
                                                         QObject *parent)
        : GraphViewModelBase(parent), m_metric(metric), m_series(new SeriesModel(this)), m_xAxis(&sharedXAxis) {
    }

    QList<SeriesModel *> BenchmarkHistoryViewModel::series(const QList<int> &columns) const {
        QList<SeriesModel *> result;
        if (!m_hasData) return result;
        for (const int column: columns)
            if (column == Value) result.append(m_series);
        return result;
    }

    ValueAxis BenchmarkHistoryViewModel::rankYAxis(const std::vector<QString> &tierNames) {
        ValueTransform transform;
        transform.formatter = [tierNames](const qreal value) -> QString {
            const qreal rounded = std::round(value);
            if (std::abs(value - rounded) < 1e-6 && rounded >= 0.0 &&
                rounded <= static_cast<qreal>(tierNames.size())) {
                const int index = static_cast<int>(rounded);
                if (index == 0) return QObject::tr("Unranked");
                return tierNames[static_cast<std::size_t>(index - 1)];
            }
            return ValueTransform().format(value);
        };
        ValueAxis::Options options;
        options.baseline = ValueAxis::Baseline::Zero;
        options.integral = true;
        ValueAxis axis(options, transform);
        axis.setRange(0.0, static_cast<qreal>(tierNames.size()));
        return axis;
    }

    void BenchmarkHistoryViewModel::update(const QList<QPointF> &rawPoints, const bool present,
                                           const std::vector<QString> &rankTierNames) {
        m_hasData = present && !rawPoints.isEmpty();

        if (!m_hasData) {
            m_series->points.clear();
            emit dataUpdated();
            emit boundsChanged();
            return;
        }

        m_series->setId(QString::number(Value));
        m_series->setColumn(Value);

        if (m_metric == AverageRank) {
            m_series->setName(tr("Personal-best average rank"));
            m_series->setColor(kRankColor);
            m_series->transform = ValueTransform::identity();
            m_series->points = rawPoints;
            m_series->yAxis = rankYAxis(rankTierNames);
        } else {
            m_series->setName(tr("Three-day average playtime"));
            m_series->setColor(kPlaytimeColor);
            m_series->transform = ValueTransform::secondsToMinutes();
            m_series->yAxisOptions = {.baseline = ValueAxis::Baseline::Zero};
            m_series->setData(rawPoints);
        }

        emit dataUpdated();
        emit boundsChanged();
    }
}
