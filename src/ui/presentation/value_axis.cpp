#include "value_axis.h"

#include <algorithm>
#include <cmath>
#include <utility>

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

    ValueAxis::ValueAxis() : ValueAxis(Options{}, ValueTransform::identity()) {}

    ValueAxis::ValueAxis(const Options options) : ValueAxis(options, ValueTransform::identity()) {}

    ValueAxis::ValueAxis(const Options options, ValueTransform transform)
        : m_options(options), m_transform(std::move(transform)) {}

    bool ValueAxis::setRange(const qreal dataLo, const qreal dataHi) {
        m_dataLo = dataLo;
        m_dataHi = dataHi;
        return recompute();
    }

    bool ValueAxis::setOptions(const Options options) {
        m_options = options;
        return recompute();
    }

    void ValueAxis::setTransform(ValueTransform transform) {
        m_transform = std::move(transform);
        recompute();
    }

    bool ValueAxis::recompute() {
        qreal lo = m_options.baseline == Baseline::Zero ? 0.0 : m_dataLo;
        qreal hi = m_dataHi;

        // Widen a degenerate/empty range so the axis is never zero-width.
        if (hi <= lo) {
            if (m_options.baseline == Baseline::Zero) {
                hi = 2.0 * m_options.fallbackSpan;
            } else {
                const qreal centre = lo;
                lo = centre - m_options.fallbackSpan;
                hi = centre + m_options.fallbackSpan;
            }
        }

        const int intervals = std::max(1, m_options.targetTicks);
        qreal step = niceNum(niceNum(hi - lo, false) / intervals, true);
        if (m_options.integral) step = std::max(1.0, std::round(step));

        // Epsilon handles non-representable values (e.g. 0.05) that round incorrectly
        constexpr qreal kEps = 1e-9;
        qreal niceMin = std::floor(lo / step + kEps) * step;
        const qreal niceMax = std::ceil(hi / step - kEps) * step;
        if (m_options.baseline == Baseline::Zero) niceMin = 0.0;

        QList<qreal> ticks;
        // Index from niceMin instead of accumulating step to avoid floating-point drift
        const int tickCount = int(std::round((niceMax - niceMin) / step));
        ticks.reserve(tickCount + 1);
        for (int i = 0; i <= tickCount; ++i) ticks.append(niceMin + step * i);

        return replaceResolvedState(niceMin, niceMax, std::move(ticks), m_transform);
    }
}
