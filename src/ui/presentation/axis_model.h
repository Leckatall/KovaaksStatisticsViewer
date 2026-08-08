//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_AXIS_MODEL_H
#define KOVAAKSSTATSVIEWER_AXIS_MODEL_H

#include <QList>
#include <QString>
#include <QtGlobal>
#include <utility>

#include "value_transform.h"

namespace ksv::presentation {
    class AxisModel {
    public:
        enum class Baseline {
            Zero,    // pin the lower bound to 0 (e.g. Time, Playtime)
            HugData, // round the data's min DOWN to a nice value
        };

        struct Options {
            Baseline baseline = Baseline::HugData;
            // Constrain the step (and hence every tick) to whole numbers. Used
            // by axes whose values are integral - seconds, calendar days - so
            // ticks never land on fractional values a formatter would collapse
            // into duplicate labels.
            bool integral = false;
            // Approximate number of tick intervals; the nice-number rounding
            // means the realized count varies by +-1 or so.
            int targetTicks = 10;
            // Half-width used to synthesize a range when the data range is
            // degenerate (hi <= lo) or empty, so the axis is never zero-width.
            qreal fallbackSpan = 1.0;
        };

        // Default axis spans [0, 1] with endpoint ticks - a sane placeholder
        // for array storage before real data arrives.
        AxisModel() = default;

        [[nodiscard]] static AxisModel forRange(qreal dataLo, qreal dataHi, Options options);
        [[nodiscard]] static AxisModel forRange(qreal dataLo, qreal dataHi) {
            return forRange(dataLo, dataHi, Options{});
        }

        [[nodiscard]] qreal min() const { return m_min; }
        [[nodiscard]] qreal max() const { return m_max; }
        [[nodiscard]] const QList<qreal> &ticks() const { return m_ticks; }

        // How values on this axis are displayed - shared by tick labels and
        // any tooltip reporting a value against this axis.
        ValueTransform delegate = ValueTransform::identity();

        [[nodiscard]] AxisModel withDelegate(ValueTransform d) const {
            AxisModel axis = *this;
            axis.delegate = std::move(d);
            return axis;
        }

        [[nodiscard]] QString formatTick(const qreal displayValue) const { return delegate.format(displayValue); }

        // Rect-agnostic value<->position mapping, shared by drawing, axis
        // painting, and hit-testing. `t` is a fraction of the axis span,
        // 0 at min and 1 at max; a zero-width axis maps everything to 0.5.
        [[nodiscard]] qreal normalizedPosition(const qreal value) const {
            const qreal span = m_max - m_min;
            return span != 0.0 ? (value - m_min) / span : 0.5;
        }

        [[nodiscard]] qreal valueAt(const qreal t) const { return m_min + t * (m_max - m_min); }

    private:
        qreal m_min = 0.0;
        qreal m_max = 1.0;
        QList<qreal> m_ticks{0.0, 1.0};
    };
}

#endif //KOVAAKSSTATSVIEWER_AXIS_MODEL_H
