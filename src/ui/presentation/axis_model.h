//
// Created by Lecka on 08/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_AXIS_MODEL_H
#define KOVAAKSSTATSVIEWER_AXIS_MODEL_H

#include <QList>
#include <QtGlobal>

namespace ksv::presentation {
    // A single graph axis: given a raw data range, it picks "nice" endpoints
    // and a "nice" tick step (multiples of 1/2/2.5/5 x 10^n, the classic
    // Heckbert algorithm) so tick labels land on round numbers instead of
    // arbitrary fractions of a padded range. Owns both the bounds and the tick
    // positions because the two only come out round when chosen together.
    //
    // This is the pure data/math half of an axis; AxisRenderer paints it.
    // Qt-light on purpose (only qreal/QList) so it can be unit-tested without a
    // running Quick scene.
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

    private:
        qreal m_min = 0.0;
        qreal m_max = 1.0;
        QList<qreal> m_ticks{0.0, 1.0};
    };
}

#endif //KOVAAKSSTATSVIEWER_AXIS_MODEL_H
