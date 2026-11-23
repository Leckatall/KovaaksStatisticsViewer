#ifndef KOVAAKSSTATSVIEWER_VALUE_AXIS_H
#define KOVAAKSSTATSVIEWER_VALUE_AXIS_H

#include <QtGlobal>

#include "presentation/graphs/common/axes/axis.h"
#include "presentation/graphs/common/axes/value_delegate.h"

namespace ksv::presentation {
    // Persistent Heckbert-style numeric axis. Options and delegate are configured once; setRange()
    // recomputes and commits the resolved bounds/ticks in place, preserving the configured policy.
    // By default tick spacing is never finer than the delegate's axis labels can tell apart: when two
    // adjacent ticks would label the same (e.g. "1 min" twice for sub-minute steps), the step is
    // coarsened to the next nice number until every adjacent pair of labels differs.
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
            // it, so no tick labels a value the data does not reach (e.g. run 0).
            bool niceBounds = true;
            // false: ticks stay where the step puts them even when neighbours share a label, giving up
            // the coarsening described above. graph_vm.cpp builds Options positionally up to
            // fallbackSpan, so new members go after this one.
            bool distinctLabels = true;
            // > 0: the tick step, used as given; `targetTicks`, `integral` and `distinctLabels` are
            // then ignored.
            qreal step = 0.0;
        };

        // GCC and Clang reject a default argument that needs Options' default member initializers
        // before this class is complete (`Options options = {}`, CWG 1397); these overloads delegate instead.
        ValueAxis();
        explicit ValueAxis(Options options);
        ValueAxis(Options options, ValueDelegate delegate);

        // Returns true if the resolved range changed; callers that don't care may ignore it.
        bool setRange(qreal dataLo, qreal dataHi);
        [[nodiscard]] bool setOptions(Options options);
        void setDelegate(ValueDelegate delegate);
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
        ValueDelegate m_delegate;
        qreal m_dataLo = 0.0;
        qreal m_dataHi = 1.0;
    };
}

#endif //KOVAAKSSTATSVIEWER_VALUE_AXIS_H
