#ifndef KOVAAKSSTATSVIEWER_AXIS_POLICY_H
#define KOVAAKSSTATSVIEWER_AXIS_POLICY_H

#include <map>
#include <span>
#include <string>
#include <string_view>

#include "app/contracts/graph_configuration.h"
#include "presentation/graphs/common/series/series_model.h"
#include "presentation/graphs/common/axes/value_axis.h"
#include "presentation/graphs/common/axes/value_transform.h"

namespace ksv::presentation {
    // Shape of GraphPreferences::axisPolicies: stable axis key -> policy, in displayed units.
    using AxisPolicyMap = std::map<std::string, application::AxisPolicy, std::less<>>;

    // The stored policy for `axisKey`, else the graph's default for it; FitVisibleData for an axis the
    // graph does not offer.
    [[nodiscard]] application::AxisPolicy axisPolicyFor(const AxisPolicyMap &policies, application::GraphKey graph,
                                                        std::string_view axisKey);

    // `base` with the baseline/bounds behaviour the policy dictates; its tick options are kept.
    [[nodiscard]] ValueAxis::Options optionsForPolicy(const application::AxisPolicy &policy,
                                                      ValueAxis::Options base);

    // The shared Y axis of `members` (only the series currently visible on that axis) under `policy`.
    // Fixed bounds that are not finite or not strictly increasing fit the data instead.
    [[nodiscard]] ValueAxis axisForPolicy(std::span<const SeriesModel *const> members,
                                          const application::AxisPolicy &policy, ValueAxis::Options base,
                                          const ValueTransform &transform);

    // Sets the Y axis of a series that has its axis to itself.
    void applyAxisPolicy(SeriesModel &series, const application::AxisPolicy &policy);
}

#endif //KOVAAKSSTATSVIEWER_AXIS_POLICY_H
