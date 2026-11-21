#ifndef KOVAAKSSTATSVIEWER_RANK_HISTORY_H
#define KOVAAKSSTATSVIEWER_RANK_HISTORY_H

#include <chrono>
#include <optional>
#include <span>
#include <vector>

namespace ksv::domain {
    struct AverageRankOfDay { std::chrono::sys_days day; double averageRank = 0.0; };

    struct AverageRankAfterRun {
        std::chrono::sys_seconds start;
        /// The run's 1-based position among all of the benchmark's member runs, oldest first.
        int benchmarkRun = 0;
        double averageRank = 0.0;

        [[nodiscard]] std::chrono::sys_days day() const { return std::chrono::floor<std::chrono::days>(start); }
    };

    // A window onto a retrospective rank history that was evaluated in full, so every value in it is
    // the one the whole history gave at that point.
    template<typename Point>
    struct RankHistoryWindow {
        // The last point before the window opens: the rank already held when it opens.
        std::optional<Point> established;
        std::vector<Point> points;
    };

    /// Benchmark run numbers, both inclusive.
    struct RunSpan {
        int first = 0;
        int last = 0;
    };

    /**
     * @param history Oldest first, one point per day.
     * @param first,last Inclusive.
     */
    [[nodiscard]] RankHistoryWindow<AverageRankOfDay> rankHistoryWindow(std::span<const AverageRankOfDay> history,
                                                                        std::chrono::sys_days first,
                                                                        std::chrono::sys_days last);

    /** @param history Oldest first, as in Benchmark::averageRankHistory() or Benchmark::recentAverageRankHistory(). */
    [[nodiscard]] RankHistoryWindow<AverageRankAfterRun> rankHistoryWindow(std::span<const AverageRankAfterRun> history,
                                                                           RunSpan span);

    /** @return The last point of each UTC day. */
    [[nodiscard]] std::vector<AverageRankOfDay> rankHistoryByDay(std::span<const AverageRankAfterRun> history);

    /**
     * @param memberRunStarts Chronological, as Benchmark::memberRunStarts().
     * @param first,last Inclusive UTC days.
     * @return The member runs on those days, or nothing when there are none.
     */
    [[nodiscard]] std::optional<RunSpan> runSpanOfDays(std::span<const std::chrono::sys_seconds> memberRunStarts,
                                                       std::chrono::sys_days first, std::chrono::sys_days last);
}

#endif // KOVAAKSSTATSVIEWER_RANK_HISTORY_H
