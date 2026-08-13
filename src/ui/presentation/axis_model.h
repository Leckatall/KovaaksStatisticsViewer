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
            Zero,    // Pin lower bound to 0
            HugData, // Round data min DOWN to nice value
        };

        struct Options {
            Baseline baseline = Baseline::HugData;
            bool integral = false;          // Constrain ticks to whole numbers
            int targetTicks = 10;           // Approximate tick interval count
            qreal fallbackSpan = 1.0;       // Range padding for degenerate/empty data
        };

        // TODO(2026-08-13): Extract DateTimeAxis when another axis needs calendar behavior; keeping it here preserves
        //  the value-based GraphViewModelBase/GraphCanvas contract until polymorphic axis ownership is justified.
        struct DateTimeOptions {
            int targetTicks = 10;
        };

        AxisModel() = default;

        [[nodiscard]] static AxisModel forRange(qreal dataLo, qreal dataHi, Options options);
        [[nodiscard]] static AxisModel forRange(qreal dataLo, qreal dataHi) {
            return forRange(dataLo, dataHi, Options{});
        }
        [[nodiscard]] static AxisModel forDateTimeRange(qreal dataLoMs, qreal dataHiMs, DateTimeOptions options);
        [[nodiscard]] static AxisModel forDateTimeRange(qreal dataLoMs, qreal dataHiMs) {
            return forDateTimeRange(dataLoMs, dataHiMs, DateTimeOptions{});
        }

        [[nodiscard]] qreal min() const { return m_min; }
        [[nodiscard]] qreal max() const { return m_max; }
        [[nodiscard]] const QList<qreal> &ticks() const { return m_ticks; }

        // Formats values for display (axis labels, tooltips)
        ValueTransform delegate = ValueTransform::identity();

        [[nodiscard]] AxisModel withDelegate(ValueTransform d) const {
            AxisModel axis = *this;
            axis.delegate = std::move(d);
            return axis;
        }

        [[nodiscard]] QString formatTick(const qreal displayValue) const { return delegate.format(displayValue); }

        // Maps value to [0,1] position on axis span; zero span maps to 0.5
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
