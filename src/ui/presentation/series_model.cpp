//
// Created by Lecka on 08/08/2026.
//

#include "series_model.h"

#include <limits>

namespace ksv::presentation {
    std::optional<QPointF> SeriesModel::sampleAtX(const qreal xValue) const {
        if (points.isEmpty()) return std::nullopt;

        qreal bestDist = std::numeric_limits<qreal>::max();
        int bestIndex = -1;
        for (int i = 0; i < points.size(); ++i) {
            const qreal dist = std::abs(points[i].x() - xValue);
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = i;
            }
        }
        if (bestIndex < 0) return std::nullopt;
        return QPointF(points[bestIndex].x(), transform.display(points[bestIndex].y()));
    }
}
