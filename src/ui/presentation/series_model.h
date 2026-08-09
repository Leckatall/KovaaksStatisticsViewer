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
#include <optional>

#include "axis_model.h"
#include "value_transform.h"

namespace ksv::presentation {
    struct SeriesModel {
        QString name;
        QColor color;
        ValueTransform transform;
        AxisModel::Options yAxisOptions;
        QList<QPointF> points;
        std::optional<AxisModel> xAxis;
        std::optional<AxisModel> yAxis;

        // Stores rawPoints and rebuilds yAxis from their display-space range
        void setData(QList<QPointF> rawPoints) {
            points = std::move(rawPoints);
            qreal lo = 0.0, hi = 1.0;
            if (!points.isEmpty()) {
                lo = hi = transform.display(points.front().y());
                for (const auto &p: points) {
                    const qreal v = transform.display(p.y());
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
            yAxis = AxisModel::forRange(lo, hi, yAxisOptions).withDelegate(transform);
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
}

#endif //KOVAAKSSTATSVIEWER_SERIES_MODEL_H
