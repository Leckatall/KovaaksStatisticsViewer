//
// Created by Lecka on 07/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_MONOTONE_SPLINE_H
#define KOVAAKSSTATSVIEWER_MONOTONE_SPLINE_H

#include <QPointF>
#include <QVector>
#include <cmath>

namespace ksv::presentation {
    // Fritsch-Carlson monotone cubic interpolation: never overshoots data range between points (unlike Catmull-Rom)
    inline QVector<QPointF> monotoneCubicInterpolate(const QVector<QPointF> &points, const int samplesPerSegment = 16) {
        const int n = points.size();
        if (n < 2 || samplesPerSegment < 1) return points;

        QVector<double> dx(n - 1), m(n - 1);
        for (int i = 0; i < n - 1; ++i) {
            dx[i] = points[i + 1].x() - points[i].x();
            m[i] = dx[i] != 0.0 ? (points[i + 1].y() - points[i].y()) / dx[i] : 0.0;
        }

        // Tangent: average of adjacent secants, zero at extrema to prevent overshoot
        QVector<double> t(n);
        t[0] = m[0];
        t[n - 1] = m[n - 2];
        for (int i = 1; i < n - 1; ++i) {
            if (m[i - 1] == 0.0 || m[i] == 0.0 || (m[i - 1] > 0.0) != (m[i] > 0.0)) {
                t[i] = 0.0;
            } else {
                t[i] = (m[i - 1] + m[i]) / 2.0;
            }
        }

        // Fritsch-Carlson: rescale tangents to prevent segment overshoot
        for (int i = 0; i < n - 1; ++i) {
            if (m[i] == 0.0) {
                t[i] = 0.0;
                t[i + 1] = 0.0;
                continue;
            }
            const double alpha = t[i] / m[i];
            const double beta = t[i + 1] / m[i];
            const double normSq = alpha * alpha + beta * beta;
            if (normSq > 9.0) {
                const double tau = 3.0 / std::sqrt(normSq);
                t[i] = tau * alpha * m[i];
                t[i + 1] = tau * beta * m[i];
            }
        }

        QVector<QPointF> result;
        result.reserve((n - 1) * samplesPerSegment + 1);
        for (int i = 0; i < n - 1; ++i) {
            const QPointF &p0 = points[i];
            const QPointF &p1 = points[i + 1];
            // Zero-width segment (duplicate x) can't be sampled; just emit start point
            const int samples = dx[i] == 0.0 ? 1 : samplesPerSegment;
            for (int s = 0; s < samples; ++s) {
                const double u = double(s) / double(samples);
                const double u2 = u * u;
                const double u3 = u2 * u;
                const double h00 = 2 * u3 - 3 * u2 + 1;
                const double h10 = u3 - 2 * u2 + u;
                const double h01 = -2 * u3 + 3 * u2;
                const double h11 = u3 - u2;
                const double y = h00 * p0.y() + h10 * dx[i] * t[i] + h01 * p1.y() + h11 * dx[i] * t[i + 1];
                const double x = p0.x() + u * dx[i];
                result.append(QPointF(x, y));
            }
        }
        result.append(points.last());
        return result;
    }
}

#endif //KOVAAKSSTATSVIEWER_MONOTONE_SPLINE_H
