#ifndef KOVAAKSSTATSVIEWER_ROLLING_PLAYTIME_H
#define KOVAAKSSTATSVIEWER_ROLLING_PLAYTIME_H

#include <chrono>
#include <span>
#include <utility>
#include <vector>

#include "run.h"

namespace ksv::domain {
    /**
     * @param chronological Runs ordered by start time. Non-positive timestamps are skipped.
     * @return One sample per day with play inside its window, oldest first.
     */
    [[nodiscard]] std::vector<std::pair<std::chrono::sys_days, double>>
    rollingPlaytime(std::span<const TimedScore> chronological, int windowDays);
}

#endif // KOVAAKSSTATSVIEWER_ROLLING_PLAYTIME_H
