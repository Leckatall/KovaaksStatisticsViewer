#pragma once

#include <chrono>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "graph_configuration.h"
#include "range_projection.h"

namespace ksv::application {
    // One sample per calendar day, oldest first, as the profile's rolling-average accounting emits them.
    using PlaytimeSeries = std::vector<std::pair<std::chrono::sys_days, double> >;

    // What a playtime graph's committed window and range make of its full-history measurements.
    struct PlaytimeView {
        int windowDays = kDefaultAveragingWindowDays;
        // Set when the range resolved to calendar dates; the X axis spans exactly this, blank days included.
        std::optional<DayInterval> interval;
        // Extent of the full-history samples whatever the range, so a neighbour's default axis does not
        // shrink when this graph is narrowed.
        std::optional<DayInterval> fullExtent;

        bool operator==(const PlaytimeView &) const = default;
    };

    [[nodiscard]] int effectiveAveragingWindow(const GraphPreferences &preferences);

    [[nodiscard]] RangePreference effectiveRange(const GraphPreferences &preferences);

    [[nodiscard]] PlaytimeSeries includeInactiveDays(std::span<const std::pair<std::chrono::sys_days, double> > samples);

    /**
     * @brief Samples a projection shows, with their full-history values untouched.
     * @param full Sorted by day.
     * @return Empty for NoRelevantHistory; latest-run suffixes do not exist for playtime and show everything.
     */
    [[nodiscard]] PlaytimeSeries visiblePlaytime(std::span<const std::pair<std::chrono::sys_days, double> > full,
                                                 const RangeProjection &projection);

    [[nodiscard]] PlaytimeView describePlaytimeView(int windowDays, const RangeProjection &projection,
                                                    std::span<const std::pair<std::chrono::sys_days, double> > full);

    // The one range shape whose resolution changes when the calendar day does.
    [[nodiscard]] bool followsToday(const RangePreference &preference);
}
