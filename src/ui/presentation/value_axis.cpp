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

        // The next coarser step in the 1-2-5 nice sequence (1 -> 2 -> 5 -> 10 -> 20 ...).
        qreal nextNiceStep(const qreal step) {
            const qreal exponent = std::floor(std::log10(step) + 1e-9);
            const qreal magnitude = std::pow(10.0, exponent);
            const qreal fraction = step / magnitude;
            if (fraction < 1.5) return 2.0 * magnitude;
            if (fraction < 3.5) return 5.0 * magnitude;
            return 10.0 * magnitude;
        }

        // Upper bound on step coarsening; each round multiplies the step by at least 2, so this is
        // far beyond any realistic range and only guards a formatter that collapses every value.
        constexpr int kMaxCoarsening = 64;
        // Epsilon handles non-representable values (e.g. 0.05) that round incorrectly
        constexpr qreal kEps = 1e-9;
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

        Resolved resolved = resolve(lo, hi, step);
        for (int round = 0; round < kMaxCoarsening && !adjacentLabelsDistinct(resolved.ticks); ++round) {
            step = nextNiceStep(step);
            resolved = resolve(lo, hi, step);
        }

        return replaceResolvedState(resolved.min, resolved.max, std::move(resolved.ticks), m_transform);
    }

    ValueAxis::Resolved ValueAxis::resolve(const qreal lo, const qreal hi, const qreal step) const {
        Resolved resolved{};
        qreal first;
        qreal last;
        if (m_options.niceBounds) {
            resolved.min = m_options.baseline == Baseline::Zero ? 0.0 : std::floor(lo / step + kEps) * step;
            resolved.max = std::ceil(hi / step - kEps) * step;
            first = resolved.min;
            last = resolved.max;
        } else {
            resolved.min = lo;
            resolved.max = hi;
            first = std::ceil(lo / step - kEps) * step;
            last = std::floor(hi / step + kEps) * step;
        }

        // Index from the first tick instead of accumulating step to avoid floating-point drift
        const int tickCount = int(std::round((last - first) / step));
        if (tickCount < 0) {
            // No nice multiple falls inside an exact-bounds range: label its start alone.
            resolved.ticks = {lo};
            return resolved;
        }
        resolved.ticks.reserve(tickCount + 1);
        for (int i = 0; i <= tickCount; ++i) resolved.ticks.append(first + step * i);
        return resolved;
    }

    bool ValueAxis::adjacentLabelsDistinct(const QList<qreal> &ticks) const {
        QString previous;
        for (int i = 0; i < ticks.size(); ++i) {
            QString label = m_transform.format(ticks[i]);
            if (i > 0 && label == previous) return false;
            previous = std::move(label);
        }
        return true;
    }
}
