#include "graph_configuration.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ranges>
#include <utility>

namespace ksv::application {
    namespace {
        using namespace std::chrono;

        constexpr std::array<std::string_view, kGraphKeys.size()> kGraphKeyNames{
            "run", "dailyPlaytime", "scenarioHistory", "benchmarkRank", "benchmarkPlaytime"
        };

        constexpr RgbaColor kPlaytimeColor{77, 208, 225, 255};

        std::array<GraphDescriptor, kGraphKeys.size()> makeDescriptors() {
            return {{
                {GraphKey::Run, false, false, false, false, {}, {}},
                {
                    GraphKey::DailyPlaytime, true, false, true, false,
                    {{"playtime", "playtime", kPlaytimeColor}},
                    {{"playtime", StartAtZero{}}}
                },
                {
                    GraphKey::ScenarioHistory, true, true, false, true,
                    {
                        {"score", "score", RgbaColor{0, 150, 0, 255}},
                        {"accuracy", "accuracy", RgbaColor{0, 255, 255, 255}},
                        {"shots", "counts", RgbaColor{255, 165, 0, 255}},
                        {"hits", "counts", RgbaColor{124, 179, 66, 255}},
                        {"misses", "counts", RgbaColor{229, 57, 53, 255}},
                    },
                    {{"score", FitVisibleData{}}, {"accuracy", FitVisibleData{}}, {"counts", StartAtZero{}}}
                },
                {GraphKey::BenchmarkRank, true, false, false, false, {{"rank", "", std::nullopt}}, {}},
                {
                    GraphKey::BenchmarkPlaytime, true, false, true, false,
                    {{"playtime", "playtime", kPlaytimeColor}},
                    {{"playtime", StartAtZero{}}}
                },
            }};
        }

        class IssueSink {
        public:
            IssueSink(const GraphKey graph, std::vector<GraphConfigurationIssue> &issues)
                : m_graph(graph), m_issues(issues) {
            }

            void add(const GraphConfigurationIssueCode code, std::string group, std::string target,
                     std::string field) const {
                m_issues.push_back({code, m_graph, std::move(group), std::move(target), std::move(field)});
            }

        private:
            GraphKey m_graph;
            std::vector<GraphConfigurationIssue> &m_issues;
        };

        void validateCount(const int value, const int maximum, const IssueSink &sink, const std::string &field) {
            if (value <= 0) sink.add(GraphConfigurationIssueCode::NonPositiveCount, "dataRange", "", field);
            else if (value > maximum) sink.add(GraphConfigurationIssueCode::CountTooLarge, "dataRange", "", field);
        }

        [[nodiscard]] bool isSupportedDate(const year_month_day date) {
            return date.ok() && date >= kEarliestCalendarDate && date <= kLatestCalendarDate;
        }

        void validateRange(const GraphDescriptor &descriptor, const GraphRange &range, const IssueSink &sink) {
            std::visit([&](const auto &value) {
                using Range = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<Range, RecentDays>) {
                    validateCount(value.days, kMaximumRangeDays, sink, "recentDays");
                } else if constexpr (std::same_as<Range, LatestRuns>) {
                    if (!descriptor.latestRuns)
                        sink.add(GraphConfigurationIssueCode::NotApplicable, "dataRange", "", "latestRuns");
                    else validateCount(value.count, kMaximumLatestRuns, sink, "latestRuns");
                } else if constexpr (std::same_as<Range, CustomDates>) {
                    const bool startValid = isSupportedDate(value.start);
                    const bool endValid = isSupportedDate(value.end);
                    if (!startValid) sink.add(GraphConfigurationIssueCode::InvalidDate, "dataRange", "", "startDate");
                    if (!endValid) sink.add(GraphConfigurationIssueCode::InvalidDate, "dataRange", "", "endDate");
                    if (startValid && endValid && value.end < value.start)
                        sink.add(GraphConfigurationIssueCode::ReversedDateRange, "dataRange", "", "endDate");
                }
            }, range);
        }

        void validateBound(const double value, const IssueSink &sink, const std::string &axis, const char *field) {
            if (!std::isfinite(value)) sink.add(GraphConfigurationIssueCode::NonFiniteNumber, "axes", axis, field);
            else if (std::abs(value) > kMaximumAxisBoundMagnitude)
                sink.add(GraphConfigurationIssueCode::AxisBoundOutOfRange, "axes", axis, field);
        }

        void addSeriesDocumentIssue(std::vector<GraphConfigurationIssue> &issues, std::string target) {
            issues.push_back({
                GraphConfigurationIssueCode::InvalidRunSeries, GraphKey::Run, "graphLines", std::move(target), ""
            });
        }
    }

    std::string_view graphKeyName(const GraphKey key) {
        return kGraphKeyNames[static_cast<std::size_t>(key)];
    }

    std::optional<GraphKey> graphKeyFromName(const std::string_view name) {
        for (const auto key: kGraphKeys) if (graphKeyName(key) == name) return key;
        return std::nullopt;
    }

    const GraphDescriptor &graphDescriptor(const GraphKey key) {
        static const auto descriptors = makeDescriptors();
        return descriptors[static_cast<std::size_t>(key)];
    }

    SeriesStyleDefault defaultSeriesStyle(const GraphKey graph, const std::string_view seriesKey) {
        const auto &series = graphDescriptor(graph).series;
        const auto found = std::ranges::find(series, seriesKey, &GraphSeriesDescriptor::key);
        return {found == series.end() ? std::nullopt : found->defaultColor, kDefaultSeriesWidth};
    }

    GraphPreferences defaultGraphPreferences(const GraphKey key) {
        const auto &descriptor = graphDescriptor(key);
        GraphPreferences preferences;
        if (descriptor.dateRange) preferences.range = RangePreference{};
        if (descriptor.averagingWindow) preferences.averagingWindowDays = kDefaultAveragingWindowDays;
        preferences.appearance.lineModeMarkers = descriptor.defaultLineModeMarkers;
        for (const auto &axis: descriptor.axes) preferences.axisPolicies.emplace(axis.key, axis.defaultPolicy);
        return preferences;
    }

    bool operator==(const RunSeriesDocument &left, const RunSeriesDocument &right) {
        return sameSeriesConfigs(left.series, right.series) && left.nextSeriesId == right.nextSeriesId &&
               sameAxisConfigs(left.axes, right.axes) && left.nextAxisId == right.nextAxisId;
    }

    RunSeriesDocument defaultRunSeriesDocument() {
        return {defaultSeriesConfigs(), kFirstUserComputedSeriesId, defaultAxisConfigs(), kFirstUserAxisId};
    }

    GraphConfiguration defaultGraphConfiguration() {
        GraphConfiguration configuration;
        for (const auto key: kGraphKeys) configuration.preferences(key) = defaultGraphPreferences(key);
        configuration.runSeries = defaultRunSeriesDocument();
        return configuration;
    }

    std::vector<GraphConfigurationIssue> validateGraphPreferences(const GraphKey key,
                                                                  const GraphPreferences &preferences) {
        std::vector<GraphConfigurationIssue> issues;
        const IssueSink sink(key, issues);
        const auto &descriptor = graphDescriptor(key);

        if (!descriptor.dateRange && preferences.range)
            sink.add(GraphConfigurationIssueCode::NotApplicable, "dataRange", "", "range");
        else if (descriptor.dateRange && !preferences.range)
            sink.add(GraphConfigurationIssueCode::MissingValue, "dataRange", "", "range");
        else if (preferences.range)
            validateRange(descriptor, preferences.range->range, sink);

        if (!descriptor.averagingWindow && preferences.averagingWindowDays)
            sink.add(GraphConfigurationIssueCode::NotApplicable, "dataRange", "", "averagingWindow");
        else if (descriptor.averagingWindow && !preferences.averagingWindowDays)
            sink.add(GraphConfigurationIssueCode::MissingValue, "dataRange", "", "averagingWindow");
        else if (preferences.averagingWindowDays)
            validateCount(*preferences.averagingWindowDays, kMaximumAveragingWindowDays, sink, "averagingWindow");

        for (const auto &[seriesKey, style]: preferences.seriesStyles) {
            if (std::ranges::none_of(descriptor.series, [&](const auto &series) { return series.key == seriesKey; })) {
                sink.add(GraphConfigurationIssueCode::UnknownSeries, "seriesStyle", seriesKey, "");
                continue;
            }
            if (!style.width) continue;
            if (!std::isfinite(*style.width))
                sink.add(GraphConfigurationIssueCode::NonFiniteNumber, "seriesStyle", seriesKey, "width");
            else if (*style.width < kMinimumLineWidth || *style.width > kMaximumLineWidth)
                sink.add(GraphConfigurationIssueCode::LineWidthOutOfRange, "seriesStyle", seriesKey, "width");
        }

        for (const auto &axis: descriptor.axes)
            if (!preferences.axisPolicies.contains(axis.key))
                sink.add(GraphConfigurationIssueCode::MissingValue, "axes", std::string(axis.key), "fitMode");
        for (const auto &[axisKey, policy]: preferences.axisPolicies) {
            if (std::ranges::none_of(descriptor.axes, [&](const auto &axis) { return axis.key == axisKey; })) {
                sink.add(GraphConfigurationIssueCode::UnknownAxis, "axes", axisKey, "");
                continue;
            }
            const auto *bounds = std::get_if<FixedBounds>(&policy);
            if (!bounds) continue;
            validateBound(bounds->minimum, sink, axisKey, "fixedMin");
            validateBound(bounds->maximum, sink, axisKey, "fixedMax");
            if (std::isfinite(bounds->minimum) && std::isfinite(bounds->maximum) && bounds->minimum >= bounds->maximum)
                sink.add(GraphConfigurationIssueCode::FixedBoundsNotIncreasing, "axes", axisKey, "fixedMax");
        }
        return issues;
    }

    std::vector<GraphConfigurationIssue> validateRunSeriesDocument(const RunSeriesDocument &document) {
        std::vector<GraphConfigurationIssue> issues;
        for (const auto &error: validateSeriesConfigs(document.series)) addSeriesDocumentIssue(issues, error.path);

        std::vector<std::pair<uint64_t, std::size_t>> axisIds;
        uint64_t maximumAxisId = 0;
        for (std::size_t index = 0; index < document.axes.size(); ++index) {
            const auto id = document.axes[index].id.value;
            if (id == 0) addSeriesDocumentIssue(issues, "axes[" + std::to_string(index) + "].id");
            if (std::ranges::any_of(axisIds, [&](const auto &seen) { return seen.first == id; }))
                addSeriesDocumentIssue(issues, "axes[" + std::to_string(index) + "].id");
            axisIds.emplace_back(id, index);
            maximumAxisId = std::max(maximumAxisId, id);
        }

        uint64_t maximumSeriesId = 0;
        for (std::size_t index = 0; index < document.series.size(); ++index) {
            const auto &series = document.series[index];
            maximumSeriesId = std::max(maximumSeriesId, series.id.value);
            if (series.yAxisId && std::ranges::none_of(document.axes, [&](const auto &axis) {
                return axis.id == *series.yAxisId;
            }))
                addSeriesDocumentIssue(issues, "records[" + std::to_string(index) + "].yAxisId");
        }

        // An allocator only runs out by handing out the largest ID, which must then still be in use.
        constexpr auto kMaximumId = std::numeric_limits<uint64_t>::max();
        if (document.nextSeriesId ? document.nextSeriesId->value <= maximumSeriesId : maximumSeriesId != kMaximumId)
            addSeriesDocumentIssue(issues, "nextComputedSeriesId");
        if (document.nextAxisId ? document.nextAxisId->value <= maximumAxisId : maximumAxisId != kMaximumId)
            addSeriesDocumentIssue(issues, "nextAxisId");
        return issues;
    }

    std::vector<GraphConfigurationIssue> validateGraphConfiguration(const GraphConfiguration &configuration) {
        std::vector<GraphConfigurationIssue> issues;
        for (const auto key: kGraphKeys) {
            auto graphIssues = validateGraphPreferences(key, configuration.preferences(key));
            issues.insert(issues.end(), std::make_move_iterator(graphIssues.begin()),
                          std::make_move_iterator(graphIssues.end()));
        }
        auto seriesIssues = validateRunSeriesDocument(configuration.runSeries);
        issues.insert(issues.end(), std::make_move_iterator(seriesIssues.begin()),
                      std::make_move_iterator(seriesIssues.end()));
        return issues;
    }
}
