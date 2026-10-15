#ifndef KOVAAKSSTATSVIEWER_VALUE_AXIS_H
#define KOVAAKSSTATSVIEWER_VALUE_AXIS_H

#include <QtGlobal>

#include "axis.h"
#include "value_transform.h"

namespace ksv::presentation {
    // Persistent Heckbert-style numeric axis. Options and formatter are configured once; setRange()
    // recomputes and commits the resolved bounds/ticks in place, preserving the configured policy.
    // Tick spacing is never finer than the formatter can tell apart: when two adjacent ticks would
    // format to the same label (e.g. "1 min" twice for sub-minute steps), the step is coarsened to
    // the next nice number until every adjacent pair of labels differs.
    class ValueAxis final : public Axis {
    public:
        enum class Baseline {
            Zero, // Pin lower bound to 0
            HugData, // Round data min DOWN to nice value
        };

        struct Options {
            Baseline baseline = Baseline::HugData;
            bool integral = false; // Constrain ticks to whole numbers
            int targetTicks = 10; // Approximate tick interval count
            qreal fallbackSpan = 1.0; // Range padding for degenerate/empty data
            // true: bounds widen outwards to the enclosing nice multiples (a tick at each end).
            // false: bounds are exactly the data range and ticks are only the nice multiples inside
            // it, so no tick labels a value the data does not reach (e.g. run "#0").
            bool niceBounds = true;
        };

        // Default arguments referring to Options (a nested type of this class) trigger a GCC 11
        // MinGW bug when written inline (`Options options = {}`); these overloads delegate instead.
        ValueAxis();
        explicit ValueAxis(Options options);
        ValueAxis(Options options, ValueTransform transform);

        // Returns true if the resolved range changed; callers that don't care may ignore it.
        bool setRange(qreal dataLo, qreal dataHi);
        [[nodiscard]] bool setOptions(Options options);
        void setTransform(ValueTransform transform);
        [[nodiscard]] const Options &options() const { return m_options; }

    private:
        struct Resolved {
            qreal min;
            qreal max;
            QList<qreal> ticks;
        };

        [[nodiscard]] bool recompute();
        [[nodiscard]] Resolved resolve(qreal lo, qreal hi, qreal step) const;
        [[nodiscard]] bool adjacentLabelsDistinct(const QList<qreal> &ticks) const;

        Options m_options;
        ValueTransform m_transform;
        qreal m_dataLo = 0.0;
        qreal m_dataHi = 1.0;
    };
}

#endif //KOVAAKSSTATSVIEWER_VALUE_AXIS_H
