#ifndef KOVAAKSSTATSVIEWER_RANK_HISTORY_H
#define KOVAAKSSTATSVIEWER_RANK_HISTORY_H

#include <chrono>
#include <optional>
#include <span>
#include <vector>

#include "benchmarks/benchmark_projection.h"

namespace ksv::domain {
    // A date window onto a retrospective rank history that was evaluated in full, so every value in
    // it is the one the whole history gave that day.
    struct RankHistoryWindow {
        // The last improvement before the window opens: the rank already held on its first day.
        std::optional<AverageRankPoint> established;
        std::vector<AverageRankPoint> improvements;
    };

    /**
     * @param history Oldest first, as BenchmarkProjection::averageRankHistory.
     * @param first,last Inclusive.
     */
    [[nodiscard]] RankHistoryWindow rankHistoryWindow(std::span<const AverageRankPoint> history,
                                                      std::chrono::sys_days first, std::chrono::sys_days last);
}

#endif // KOVAAKSSTATSVIEWER_RANK_HISTORY_H
