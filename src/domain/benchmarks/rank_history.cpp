#include "rank_history.h"

#include <algorithm>

namespace ksv::domain {
    RankHistoryWindow rankHistoryWindow(const std::span<const AverageRankPoint> history,
                                        const std::chrono::sys_days first, const std::chrono::sys_days last) {
        RankHistoryWindow window;
        const auto begin = std::ranges::lower_bound(history, first, {}, &AverageRankPoint::day);
        if (begin != history.begin()) window.established = *std::prev(begin);
        const auto end = std::ranges::upper_bound(begin, history.end(), last, {}, &AverageRankPoint::day);
        window.improvements.assign(begin, end);
        return window;
    }
}
