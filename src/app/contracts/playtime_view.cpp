#include "playtime_view.h"

#include <algorithm>

namespace ksv::application {
    int effectiveAveragingWindow(const GraphPreferences &preferences) {
        return preferences.averagingWindowDays.value_or(kDefaultAveragingWindowDays);
    }

    RangePreference effectiveRange(const GraphPreferences &preferences) {
        return preferences.range.value_or(RangePreference{});
    }

    PlaytimeSeries visiblePlaytime(const std::span<const std::pair<std::chrono::sys_days, double> > full,
                                   const RangeProjection &projection) {
        if (std::holds_alternative<NoRelevantHistory>(projection)) return {};
        const auto *interval = std::get_if<DayInterval>(&projection);
        if (interval == nullptr) return {full.begin(), full.end()};

        const auto first = std::ranges::lower_bound(full, interval->first, {}, &std::pair<std::chrono::sys_days, double>::first);
        const auto last = std::ranges::upper_bound(first, full.end(), interval->last, {},
                                                   &std::pair<std::chrono::sys_days, double>::first);
        return {first, last};
    }

    PlaytimeView describePlaytimeView(const int windowDays, const RangeProjection &projection,
                                      const std::span<const std::pair<std::chrono::sys_days, double> > full) {
        PlaytimeView view;
        view.windowDays = windowDays;
        if (const auto *interval = std::get_if<DayInterval>(&projection)) view.interval = *interval;
        if (!full.empty()) view.fullExtent = PlaytimeDayExtent{full.front().first, full.back().first};
        return view;
    }

    bool followsToday(const RangePreference &preference) {
        return std::holds_alternative<RecentDays>(preference.range) && !preference.anchorToLatestRun;
    }
}
