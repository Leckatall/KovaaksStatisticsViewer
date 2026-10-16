#pragma once

#include <chrono>
#include <functional>

namespace ksv::data {
    // The current UTC calendar day, in the same epoch-day model as ScenarioRunId::startDay().
    class IDaySource {
    public:
        virtual ~IDaySource() = default;

        // The last day the source observed, so it never disagrees with what onDayChanged() announced.
        [[nodiscard]] virtual std::chrono::sys_days today() const = 0;

        // Called once per observed change with the new day, on the thread that owns the source.
        virtual void onDayChanged(std::function<void(std::chrono::sys_days)> callback) = 0;
    };
}
