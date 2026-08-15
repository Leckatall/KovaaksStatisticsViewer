//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SERIES_MODEL_H
#define KOVAAKSSTATSVIEWER_SERIES_MODEL_H

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVariantMap>
#include <algorithm>
#include <optional>
#include <span>

#include "axis_model.h"
#include "value_transform.h"

namespace ksv::presentation {
    struct SeriesModel {
        QString name;
        QColor color;
        ValueTransform transform;
        AxisModel::Options yAxisOptions;
        QList<QPointF> points;
        int column = -1;
        std::optional<AxisModel> xAxis;
        std::optional<AxisModel> yAxis;

        [[nodiscard]] std::optional<std::pair<qreal, qreal>> displayRange() const {
            if (points.isEmpty()) return std::nullopt;
            qreal lo = transform.display(points.front().y());
            qreal hi = lo;
            for (const auto &p: points) {
                const qreal value = transform.display(p.y());
                lo = std::min(lo, value);
                hi = std::max(hi, value);
            }
            return std::pair{lo, hi};
        }

        [[nodiscard]] AxisModel deriveYAxis() const {
            if (const auto range = displayRange()) {
                return AxisModel::forRange(range->first, range->second, yAxisOptions).withDelegate(transform);
            }
            return AxisModel::forRange(0.0, 1.0, yAxisOptions).withDelegate(transform);
        }

        void setData(QList<QPointF> rawPoints) {
            points = std::move(rawPoints);
            yAxis = deriveYAxis();
        }

        [[nodiscard]] QList<QPointF> displayPoints() const {
            QList<QPointF> out;
            out.reserve(points.size());
            for (const auto &p: points) out.append(QPointF(p.x(), transform.display(p.y())));
            return out;
        }

        [[nodiscard]] std::optional<QPointF> sampleAtX(qreal xValue) const;

        [[nodiscard]] QString formattedValueAtX(qreal xValue) const {
            const auto sample = sampleAtX(xValue);
            return sample ? transform.format(sample->y()) : QString();
        }
    };

    [[nodiscard]] inline AxisModel axisForSeries(const std::span<const SeriesModel *const> members,
                                                  const AxisModel::Options &options,
                                                  const ValueTransform &delegate) {
        std::optional<std::pair<qreal, qreal>> range;
        for (const SeriesModel *series: members) {
            if (!series) continue;
            const auto memberRange = series->displayRange();
            if (!memberRange) continue;
            if (!range) {
                range = memberRange;
            } else {
                range->first = std::min(range->first, memberRange->first);
                range->second = std::max(range->second, memberRange->second);
            }
        }
        if (!range) return AxisModel::forRange(0.0, 1.0, options).withDelegate(delegate);
        return AxisModel::forRange(range->first, range->second, options).withDelegate(delegate);
    }
}

#endif //KOVAAKSSTATSVIEWER_SERIES_MODEL_H
