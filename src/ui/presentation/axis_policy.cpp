#include "axis_policy.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>
#include <variant>

namespace ksv::presentation {
    namespace {
        // Validation rejects anything else upstream; this guards a caller that skipped it.
        std::optional<application::FixedBounds> usableBounds(const application::AxisPolicy &policy) {
            const auto *bounds = std::get_if<application::FixedBounds>(&policy);
            if (!bounds || !std::isfinite(bounds->minimum) || !std::isfinite(bounds->maximum) ||
                bounds->minimum >= bounds->maximum)
                return std::nullopt;
            return *bounds;
        }
    }

    application::AxisPolicy axisPolicyFor(const AxisPolicyMap &policies, const application::GraphKey graph,
                                          const std::string_view axisKey) {
        if (const auto stored = policies.find(axisKey); stored != policies.end()) return stored->second;
        const auto &axes = application::graphDescriptor(graph).axes;
        const auto offered = std::ranges::find(axes, axisKey, &application::GraphAxisDescriptor::key);
        return offered != axes.end() ? offered->defaultPolicy : application::AxisPolicy{application::FitVisibleData{}};
    }

    ValueAxis::Options optionsForPolicy(const application::AxisPolicy &policy, ValueAxis::Options base) {
        base.baseline = std::holds_alternative<application::StartAtZero>(policy) ? ValueAxis::Baseline::Zero
                                                                                 : ValueAxis::Baseline::HugData;
        base.niceBounds = !usableBounds(policy).has_value();
        return base;
    }

    ValueAxis axisForPolicy(const std::span<const SeriesModel *const> members, const application::AxisPolicy &policy,
                            const ValueAxis::Options base, const ValueTransform &transform) {
        const ValueAxis::Options options = optionsForPolicy(policy, base);
        const auto bounds = usableBounds(policy);
        if (!bounds) return axisForSeries(members, options, transform);
        ValueAxis axis(options, transform);
        std::ignore = axis.setRange(bounds->minimum, bounds->maximum);
        return axis;
    }
}
