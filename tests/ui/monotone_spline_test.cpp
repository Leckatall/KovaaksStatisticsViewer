//
// monotoneCubicInterpolate tests: the whole reason this function exists is
// to guarantee no overshoot between points (unlike QSplineSeries), so that
// property gets the most thorough coverage here.
//

#include <gtest/gtest.h>

#include "monotone_spline.h"

using namespace ksv::presentation;

namespace {
    // Asserts every sampled point's y stays within [min(y_i, y_i+1), max(y_i,
    // y_i+1)] for its enclosing segment - the defining no-overshoot property
    // of a monotone Hermite spline.
    void expectNoOvershoot(const QVector<QPointF> &points) {
        const QVector<QPointF> curve = monotoneCubicInterpolate(points, 16);
        int segment = 0;
        for (const auto &p: curve) {
            while (segment < points.size() - 2 && p.x() > points[segment + 1].x() + 1e-9) ++segment;
            const double lo = std::min(points[segment].y(), points[segment + 1].y());
            const double hi = std::max(points[segment].y(), points[segment + 1].y());
            EXPECT_GE(p.y(), lo - 1e-9) << "overshoot below segment " << segment << " at x=" << p.x();
            EXPECT_LE(p.y(), hi + 1e-9) << "overshoot above segment " << segment << " at x=" << p.x();
        }
    }

    TEST(MonotoneSplineTest, FewerThanTwoPointsReturnsInputUnchanged) {
        EXPECT_EQ(monotoneCubicInterpolate({}), QVector<QPointF>{});
        const QVector<QPointF> single{{1.0, 2.0}};
        EXPECT_EQ(monotoneCubicInterpolate(single), single);
    }

    TEST(MonotoneSplineTest, PassesThroughEveryControlPoint) {
        const QVector<QPointF> points{{0, 0}, {1, 5}, {2, 1}, {3, 8}};
        const QVector<QPointF> curve = monotoneCubicInterpolate(points, 8);

        for (const auto &control: points) {
            const bool found = std::any_of(curve.begin(), curve.end(), [&](const QPointF &p) {
                return std::abs(p.x() - control.x()) < 1e-9 && std::abs(p.y() - control.y()) < 1e-9;
            });
            EXPECT_TRUE(found) << "missing control point (" << control.x() << ", " << control.y() << ")";
        }
    }

    TEST(MonotoneSplineTest, NoOvershootOnMonotoneIncreasingData) {
        expectNoOvershoot({{0, 0}, {1, 1}, {2, 4}, {3, 4.1}, {4, 20}});
    }

    TEST(MonotoneSplineTest, NoOvershootOnMonotoneDecreasingData) {
        expectNoOvershoot({{0, 20}, {1, 15}, {2, 14.9}, {3, 5}, {4, 0}});
    }

    TEST(MonotoneSplineTest, NoOvershootAroundALocalPeak) {
        // A sharp local peak is exactly where a Catmull-Rom-style spline
        // (QSplineSeries) tends to overshoot past the peak's neighbors.
        expectNoOvershoot({{0, 0}, {1, 0}, {2, 100}, {3, 1}, {4, 1}});
    }

    TEST(MonotoneSplineTest, NoOvershootOnAFlatRun) {
        expectNoOvershoot({{0, 5}, {1, 5}, {2, 5}, {3, 5}});
    }

    TEST(MonotoneSplineTest, NoOvershootWithOnlyTwoPoints) {
        expectNoOvershoot({{0, 3}, {5, 9}});
    }

    TEST(MonotoneSplineTest, HandlesDuplicateTimestampsWithoutDividingByZero) {
        const QVector<QPointF> points{{0, 0}, {1, 5}, {1, 9}, {2, 3}};
        const QVector<QPointF> curve = monotoneCubicInterpolate(points, 8);

        for (const auto &p: curve) {
            EXPECT_TRUE(std::isfinite(p.x()));
            EXPECT_TRUE(std::isfinite(p.y()));
        }
    }
}
