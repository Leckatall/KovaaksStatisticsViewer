#include "rank_history.h"

#include <algorithm>

namespace ksv::domain {
    namespace {
        template<typename Point, typename Key, typename Projection>
        RankHistoryWindow<Point> windowOf(const std::span<const Point> history, const Key &first, const Key &last,
                                          const Projection projection) {
            RankHistoryWindow<Point> window;
            const auto begin = std::ranges::lower_bound(history, first, {}, projection);
            if (begin != history.begin()) window.established = *std::prev(begin);
            const auto end = std::ranges::upper_bound(begin, history.end(), last, {}, projection);
            window.points.assign(begin, end);
            return window;
        }
    }

    RankHistoryWindow<AverageRankOfDay> rankHistoryWindow(const std::span<const AverageRankOfDay> history,
                                                          const std::chrono::sys_days first,
                                                          const std::chrono::sys_days last) {
        return windowOf(history, first, last, &AverageRankOfDay::day);
    }

    RankHistoryWindow<AverageRankAfterRun> rankHistoryWindow(const std::span<const AverageRankAfterRun> history,
                                                             const RunSpan span) {
        return windowOf(history, span.first, span.last, &AverageRankAfterRun::benchmarkRun);
    }

    std::vector<AverageRankOfDay> rankHistoryByDay(const std::span<const AverageRankAfterRun> history) {
        std::vector<AverageRankOfDay> result;
        for (const auto &point: history) {
            if (!result.empty() && result.back().day == point.day()) result.back().averageRank = point.averageRank;
            else result.push_back({point.day(), point.averageRank});
        }
        return result;
    }

    std::optional<RunSpan> runSpanOfDays(const std::span<const std::chrono::sys_seconds> memberRunStarts,
                                         const std::chrono::sys_days first, const std::chrono::sys_days last) {
        const auto begin = std::ranges::lower_bound(memberRunStarts, std::chrono::sys_seconds{first});
        const auto end = std::ranges::lower_bound(begin, memberRunStarts.end(),
                                                  std::chrono::sys_seconds{last + std::chrono::days{1}});
        if (begin == end) return std::nullopt;
        return RunSpan{static_cast<int>(begin - memberRunStarts.begin()) + 1,
                       static_cast<int>(end - memberRunStarts.begin())};
    }
}
