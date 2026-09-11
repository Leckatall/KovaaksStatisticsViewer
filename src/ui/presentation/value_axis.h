#ifndef KOVAAKSSTATSVIEWER_VALUE_AXIS_H
#define KOVAAKSSTATSVIEWER_VALUE_AXIS_H

#include <QtGlobal>

#include "axis.h"
#include "value_transform.h"

namespace ksv::presentation {
    // Persistent Heckbert-style numeric axis. Options and formatter are configured once; setRange()
    // recomputes and commits the resolved bounds/ticks in place, preserving the configured policy.
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
        };

        // Default arguments referring to Options (a nested type of this class) trigger a GCC 11
        // MinGW bug when written inline (`Options options = {}`); these overloads delegate instead.
        ValueAxis();
        explicit ValueAxis(Options options);
        ValueAxis(Options options, ValueTransform transform);

        [[nodiscard]] bool setRange(qreal dataLo, qreal dataHi);
        [[nodiscard]] bool setOptions(Options options);
        void setTransform(ValueTransform transform);
        [[nodiscard]] const Options &options() const { return m_options; }

    private:
        [[nodiscard]] bool recompute();

        Options m_options;
        ValueTransform m_transform;
        qreal m_dataLo = 0.0;
        qreal m_dataHi = 1.0;
    };
}

#endif //KOVAAKSSTATSVIEWER_VALUE_AXIS_H
