#include "rolling_playtime.h"

#include <algorithm>

namespace ksv::domain {
    std::vector<std::pair<std::chrono::sys_days, double>>
    rollingPlaytime(const std::span<const TimedScore> chronological, const int windowDays) {
        // Input must be chronological because day totals merge only adjacent equal days.
        using namespace std::chrono;
        std::vector<std::pair<sys_days, double> > result;
        if (windowDays <= 0 || chronological.empty()) return result;

        std::vector<std::pair<sys_days, double> > daily_totals;
        for (const auto &run: chronological) {
            // Skip pre-epoch timestamps (malformed/test data) to avoid stretching axis back to 1970
            if (run.start_time <= 0) continue;
            const auto day = run.startDay();
            if (!daily_totals.empty() && daily_totals.back().first == day) {
                daily_totals.back().second += run.duration_seconds;
            } else {
                daily_totals.emplace_back(day, run.duration_seconds);
            }
        }

        if (daily_totals.empty()) return result;

        const sys_days first_day = daily_totals.front().first;
        const sys_days last_day = daily_totals.back().first;
        const auto window = days{windowDays};

        // A sample needs a day with runs inside its window, so output is bounded by both the calendar
        // span and daily_totals * window; the product alone grows with the window, not the data.
        result.reserve(std::min(static_cast<std::size_t>((last_day - first_day).count()) + 1,
                                daily_totals.size() * static_cast<std::size_t>(windowDays)));
        std::size_t lo = 0;
        std::size_t next = 0;
        double window_sum = 0.0;
        for (sys_days day = first_day; day <= last_day;) {
            if (next < daily_totals.size() && daily_totals[next].first == day) {
                window_sum += daily_totals[next].second;
                ++next;
            }
            while (lo < next && daily_totals[lo].first <= day - window) {
                window_sum -= daily_totals[lo].second;
                ++lo;
            }
            if (lo == next) {
                window_sum = 0.0;
            }
            if (window_sum != 0.0) {
                const auto window_len = std::min(static_cast<std::size_t>((day - first_day).count()) + 1,
                                                 static_cast<std::size_t>(windowDays));
                result.emplace_back(day, window_sum / static_cast<double>(window_len));
                day += days{1};
            } else {
                day = (next < daily_totals.size()) ? daily_totals[next].first : last_day + days{1};
            }
        }

        return result;
    }
}
