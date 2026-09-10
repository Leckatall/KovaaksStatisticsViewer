#include "date_axis.h"

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QTimeZone>
#include <cmath>

namespace ksv::presentation {
    qint64 epochDayToUtcMs(const long long epochDay) {
        return QDateTime(QDate(1970, 1, 1).addDays(epochDay), QTime(0, 0), QTimeZone::utc())
            .toMSecsSinceEpoch();
    }

    ValueTransform utcDayDateDelegate() {
        ValueTransform transform;
        transform.formatter = [](const qreal value) {
            return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(value)), QTimeZone::utc())
                .date()
                .toString(QStringLiteral("MMM d"));
        };
        return transform;
    }

    AxisModel utcDayAxis(const qreal loMs, const qreal hiMs, const bool present) {
        if (!present) {
            const auto nowMs = static_cast<qreal>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
            return AxisModel::forDateTimeRange(nowMs, nowMs).withDelegate(utcDayDateDelegate());
        }
        return AxisModel::forDateTimeRange(loMs, hiMs, {.targetTicks = 10}).withDelegate(utcDayDateDelegate());
    }
}
