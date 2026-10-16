#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "graph_configuration.h"

namespace ksv::application {
    // The anchor a date preset needed does not exist: no relevant run, so no history to show.
    struct NoRelevantHistory {
        bool operator==(const NoRelevantHistory &) const = default;
    };

    struct EntireHistory {
        bool operator==(const EntireHistory &) const = default;
    };

    // Inclusive at both ends; empty (not an error) when first is after last.
    struct DayInterval {
        std::chrono::sys_days first;
        std::chrono::sys_days last;

        [[nodiscard]] bool contains(std::chrono::sys_days day) const { return first <= day && day <= last; }

        bool operator==(const DayInterval &) const = default;
    };

    // The newest `count` rows of the chronological full history.
    struct LatestRunSuffix {
        int count = 0;

        bool operator==(const LatestRunSuffix &) const = default;
    };

    using RangeProjection = std::variant<NoRelevantHistory, EntireHistory, DayInterval, LatestRunSuffix>;

    /**
     * @brief Resolves a stored range preference into what a graph should show.
     * @param today The day source's current day; anchors presets when the anchor flag is off.
     * @param latestRelevantRunDay Newest day among the graph's full relevant runs, before any filtering.
     * @return Custom dates, all history and latest-N ignore the anchor flag; with the flag on and no
     *         relevant run a recent-days preset is NoRelevantHistory rather than borrowing another anchor.
     */
    [[nodiscard]] RangeProjection resolveRange(const RangePreference &preference, std::chrono::sys_days today,
                                               std::optional<std::chrono::sys_days> latestRelevantRunDay);

    /**
     * @brief Picks which rows of a chronological history a projection shows.
     * @param rowDays Calendar day of each row, oldest first.
     * @return Indices into rowDays in their original order, so row identity survives the filter.
     */
    [[nodiscard]] std::vector<std::size_t> selectRows(const RangeProjection &projection,
                                                      std::span<const std::chrono::sys_days> rowDays);

    // Order-independent; nullopt for no rows.
    [[nodiscard]] std::optional<std::chrono::sys_days> latestDay(std::span<const std::chrono::sys_days> days);

    // Same conversion as ScenarioRunId::startDay(), for sources that only hold epoch milliseconds.
    [[nodiscard]] std::chrono::sys_days epochDayOfMillis(long long epochMilliseconds);
}
