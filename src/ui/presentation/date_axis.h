#ifndef KOVAAKSSTATSVIEWER_DATE_AXIS_H
#define KOVAAKSSTATSVIEWER_DATE_AXIS_H

#include <QtGlobal>

#include "axis_model.h"
#include "value_transform.h"

namespace ksv::presentation {
    // Days since the Unix epoch -> UTC-midnight epoch milliseconds, the X unit every
    // calendar-day graph plots in.
    [[nodiscard]] qint64 epochDayToUtcMs(long long epochDay);

    // Renders an epoch-ms X value as a UTC "MMM d" day label, shared so every day graph
    // labels its axis identically.
    [[nodiscard]] ValueTransform utcDayDateDelegate();

    // Shared X axis for a UTC-day series. When `present` is false the range collapses to
    // "now" so an empty series never draws a misleading span.
    [[nodiscard]] AxisModel utcDayAxis(qreal loMs, qreal hiMs, bool present);
}

#endif // KOVAAKSSTATSVIEWER_DATE_AXIS_H
