//
// Created by Lecka on 08/08/2026.
//

#include "axis_model.h"

#include <algorithm>
#include <cmath>

namespace ksv::presentation {
    namespace {
        // Heckbert's "nice number" rounding. Returns a value close to `range`
        qreal niceNum(const qreal range, const bool round) {
            if (range <= 0.0) return 1.0;
            const qreal exponent = std::floor(std::log10(range));
            const qreal fraction = range / std::pow(10.0, exponent);

            qreal niceFraction;
            if (round) {
                if (fraction < 1.5) niceFraction = 1.0;
                else if (fraction < 3.0) niceFraction = 2.0;
                else if (fraction < 7.0) niceFraction = 5.0;
                else niceFraction = 10.0;
            } else {
                if (fraction <= 1.0) niceFraction = 1.0;
                else if (fraction <= 2.0) niceFraction = 2.0;
                else if (fraction <= 5.0) niceFraction = 5.0;
                else niceFraction = 10.0;
            }
            return niceFraction * std::pow(10.0, exponent);
        }
    }

    AxisModel AxisModel::forRange(const qreal dataLo, const qreal dataHi, const Options options) {
        qreal lo = options.baseline == Baseline::Zero ? 0.0 : dataLo;
        qreal hi = dataHi;

        // Widen a degenerate/empty range so the axis is never zero-width.
        if (hi <= lo) {
            if (options.baseline == Baseline::Zero) {
                lo = 0.0;
                hi = std::max(dataHi, 0.0) + 2.0 * options.fallbackSpan;
            } else {
                const qreal centre = lo;
                lo = centre - options.fallbackSpan;
                hi = centre + options.fallbackSpan;
            }
        }

        const int intervals = std::max(1, options.targetTicks);
        qreal step = niceNum(niceNum(hi - lo, false) / intervals, true);
        if (options.integral) step = std::max(1.0, std::round(step));

        // Epsilon handles non-representable values (e.g. 0.05) that round incorrectly
        constexpr qreal kEps = 1e-9;
        qreal niceMin = std::floor(lo / step + kEps) * step;
        qreal niceMax = std::ceil(hi / step - kEps) * step;
        if (options.baseline == Baseline::Zero) niceMin = 0.0;

        AxisModel axis;
        axis.m_min = niceMin;
        axis.m_max = niceMax;

        axis.m_ticks.clear();
        // Index from niceMin instead of accumulating step to avoid floating-point drift
        const int tickCount = int(std::round((niceMax - niceMin) / step));
        for (int i = 0; i <= tickCount; ++i) {
            axis.m_ticks.append(niceMin + step * i);
        }

        return axis;
    }
}
