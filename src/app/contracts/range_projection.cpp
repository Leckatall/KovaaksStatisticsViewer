#include "range_projection.h"

#include <algorithm>
#include <numeric>

namespace ksv::application {
    using std::chrono::days;
    using std::chrono::sys_days;

    RangeProjection resolveRange(const RangePreference &preference, const sys_days today,
                                 const std::optional<sys_days> latestRelevantRunDay) {
        struct Visitor {
            const RangePreference &preference;
            sys_days today;
            std::optional<sys_days> latestRelevantRunDay;

            RangeProjection operator()(const AllHistory &) const { return EntireHistory{}; }

            RangeProjection operator()(const RecentDays &recent) const {
                const auto anchor = preference.anchorToLatestRun ? latestRelevantRunDay : std::optional{today};
                if (!anchor) return NoRelevantHistory{};
                return DayInterval{*anchor - days{recent.days - 1}, *anchor};
            }

            RangeProjection operator()(const CustomDates &custom) const {
                return DayInterval{sys_days{custom.start}, sys_days{custom.end}};
            }

            RangeProjection operator()(const LatestRuns &latest) const { return LatestRunSuffix{latest.count}; }
        };
        return std::visit(Visitor{preference, today, latestRelevantRunDay}, preference.range);
    }

    std::vector<std::size_t> selectRows(const RangeProjection &projection, const std::span<const sys_days> rowDays) {
        std::vector<std::size_t> selected;
        struct Visitor {
            std::span<const sys_days> rowDays;
            std::vector<std::size_t> &selected;

            void operator()(const NoRelevantHistory &) const {}

            void operator()(const EntireHistory &) const {
                selected.resize(rowDays.size());
                std::iota(selected.begin(), selected.end(), std::size_t{0});
            }

            void operator()(const DayInterval &interval) const {
                for (std::size_t index = 0; index < rowDays.size(); ++index) {
                    if (interval.contains(rowDays[index])) selected.push_back(index);
                }
            }

            void operator()(const LatestRunSuffix &suffix) const {
                const auto count = std::min(rowDays.size(), static_cast<std::size_t>(std::max(suffix.count, 0)));
                selected.resize(count);
                std::iota(selected.begin(), selected.end(), rowDays.size() - count);
            }
        };
        std::visit(Visitor{rowDays, selected}, projection);
        return selected;
    }

    std::optional<sys_days> latestDay(const std::span<const sys_days> days) {
        if (days.empty()) return std::nullopt;
        return *std::ranges::max_element(days);
    }

    sys_days epochDayOfMillis(const long long epochMilliseconds) {
        using namespace std::chrono;
        return floor<std::chrono::days>(sys_time<milliseconds>{milliseconds{epochMilliseconds}});
    }
}
