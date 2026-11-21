#ifndef KOVAAKSSTATSVIEWER_RUN_DATE_AXIS_H
#define KOVAAKSSTATSVIEWER_RUN_DATE_AXIS_H

#include <QtGlobal>

#include <optional>
#include <utility>
#include <vector>

#include "presentation/graphs/common/axes/axis.h"

namespace ksv::presentation {
    // An axis of run numbers, one unit per run, labelled with UTC dates. Ticks sit on the first visible
    // run of each calendar period, for the shortest day-or-longer period that keeps them within the tick
    // target; the first visible run always has one, and a tick too few runs after the previous kept one
    // is dropped so sparse periods do not bunch together.
    class RunDateAxis final : public Axis {
    public:
        struct Options {
            int targetTicks = 10;
        };

        // GCC and Clang reject a default argument that needs Options' default member initializers
        // before this class is complete (CWG 1397); this overload delegates instead.
        RunDateAxis();
        explicit RunDateAxis(Options options);

        /**
         * @param startsMs Chronological; entry `i` is the UTC start of run `firstRun + i`.
         * @param calendarRange The day range the runs were chosen for, reported by calendarRangeMs().
         * @return Whether the bounds or ticks changed.
         */
        [[nodiscard]] bool setRuns(std::vector<qint64> startsMs, int firstRun,
                                   std::optional<std::pair<qint64, qint64>> calendarRange);
        [[nodiscard]] std::optional<std::pair<qint64, qint64>> calendarRangeMs() const override;

    private:
        Options m_options;
        std::optional<std::pair<qint64, qint64>> m_calendarRange;
    };
}

#endif // KOVAAKSSTATSVIEWER_RUN_DATE_AXIS_H
