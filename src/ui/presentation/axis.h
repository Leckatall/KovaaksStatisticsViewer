#ifndef KOVAAKSSTATSVIEWER_AXIS_H
#define KOVAAKSSTATSVIEWER_AXIS_H

#include <QList>
#include <QString>
#include <QtGlobal>
#include <utility>

#include "value_transform.h"

namespace ksv::presentation {
    // Read-only rendering surface shared by every axis kind. Bounds, ticks, and formatter are only
    // ever replaced together via replaceResolvedState() so they can never diverge from each other.
    class Axis {
    public:
        virtual ~Axis() = default;

        [[nodiscard]] qreal min() const { return m_min; }
        [[nodiscard]] qreal max() const { return m_max; }
        [[nodiscard]] const QList<qreal> &ticks() const { return m_ticks; }
        [[nodiscard]] QString formatTick(const qreal value) const { return m_transform.format(value); }

        // Maps value to [0,1] position on axis span; zero span maps to 0.5
        [[nodiscard]] qreal normalizedPosition(const qreal value) const {
            const qreal span = m_max - m_min;
            return span != 0.0 ? (value - m_min) / span : 0.5;
        }

        [[nodiscard]] qreal valueAt(const qreal position) const { return m_min + position * (m_max - m_min); }

    protected:
        Axis() = default;

        // Commits bounds, ticks, and formatter as one operation and reports whether the resolved
        // geometry (bounds or ticks) changed. The ValueTransform held is a std::function and is
        // never compared for that determination.
        [[nodiscard]] bool replaceResolvedState(const qreal min, const qreal max, QList<qreal> ticks,
                                                ValueTransform transform) {
            const bool boundsChanged = min != m_min || max != m_max || ticks != m_ticks;
            m_min = min;
            m_max = max;
            m_ticks = std::move(ticks);
            m_transform = std::move(transform);
            return boundsChanged;
        }

    private:
        qreal m_min = 0.0;
        qreal m_max = 1.0;
        QList<qreal> m_ticks{0.0, 1.0};
        ValueTransform m_transform = ValueTransform::identity();
    };
}

#endif //KOVAAKSSTATSVIEWER_AXIS_H
