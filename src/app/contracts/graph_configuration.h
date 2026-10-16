#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "series_config.h"

namespace ksv::application {
    // Preference identity is the graph type alone: never a selected run, scenario or benchmark.
    enum class GraphKey {
        Run,
        DailyPlaytime,
        ScenarioHistory,
        BenchmarkRank,
        BenchmarkPlaytime
    };

    inline constexpr std::array kGraphKeys{
        GraphKey::Run,
        GraphKey::DailyPlaytime,
        GraphKey::ScenarioHistory,
        GraphKey::BenchmarkRank,
        GraphKey::BenchmarkPlaytime
    };

    [[nodiscard]] std::string_view graphKeyName(GraphKey key);

    [[nodiscard]] std::optional<GraphKey> graphKeyFromName(std::string_view name);

    // ── Stated input limits ──────────────────────────────────────────────────────────────────────
    inline constexpr int kMaximumRangeDays = 36500;
    inline constexpr int kMaximumLatestRuns = 1'000'000;
    inline constexpr int kMaximumAveragingWindowDays = 3650;
    inline constexpr double kMaximumAxisBoundMagnitude = 1e12;
    inline constexpr std::chrono::year_month_day kEarliestCalendarDate{
        std::chrono::year{1970}, std::chrono::January, std::chrono::day{1}
    };
    inline constexpr std::chrono::year_month_day kLatestCalendarDate{
        std::chrono::year{9999}, std::chrono::December, std::chrono::day{31}
    };

    // ── Ranges ───────────────────────────────────────────────────────────────────────────────────
    struct AllHistory {
        bool operator==(const AllHistory &) const = default;
    };

    struct RecentDays {
        int days = 0;

        bool operator==(const RecentDays &) const = default;
    };

    // Inclusive at both ends, in the UTC calendar-day model of ScenarioRunId::startDay().
    struct CustomDates {
        std::chrono::year_month_day start;
        std::chrono::year_month_day end;

        bool operator==(const CustomDates &) const = default;
    };

    // Scenario history only.
    struct LatestRuns {
        int count = 0;

        bool operator==(const LatestRuns &) const = default;
    };

    using GraphRange = std::variant<AllHistory, RecentDays, CustomDates, LatestRuns>;

    struct RangePreference {
        GraphRange range = AllHistory{};
        // Kept while all history or custom dates are selected, so it survives switching back to a preset.
        bool anchorToLatestRun = false;

        bool operator==(const RangePreference &) const = default;
    };

    // ── Appearance ───────────────────────────────────────────────────────────────────────────────
    enum class ConnectionMode {
        Smooth,
        Straight,
        PointsOnly
    };

    struct LineAppearance {
        ConnectionMode connection = ConnectionMode::Smooth;
        // The line-mode preference; PointsOnly draws markers without overwriting it.
        bool lineModeMarkers = false;

        bool operator==(const LineAppearance &) const = default;
    };

    // An absent field keeps the graph's default for that series (see defaultSeriesStyle()).
    struct SeriesStyleOverride {
        std::optional<RgbaColor> color;
        std::optional<double> width;

        bool operator==(const SeriesStyleOverride &) const = default;
    };

    // ── Numeric Y-axis policies, in displayed units ─────────────────────────────────────────────
    struct FitVisibleData {
        bool operator==(const FitVisibleData &) const = default;
    };

    struct StartAtZero {
        bool operator==(const StartAtZero &) const = default;
    };

    struct FixedBounds {
        double minimum = 0.0;
        double maximum = 0.0;

        bool operator==(const FixedBounds &) const = default;
    };

    using AxisPolicy = std::variant<FitVisibleData, StartAtZero, FixedBounds>;

    // ── Per-graph preferences ────────────────────────────────────────────────────────────────────
    // One shape for every graph; graphDescriptor() says which fields a graph offers and
    // validateGraphPreferences() rejects the rest.
    struct GraphPreferences {
        std::optional<RangePreference> range;
        std::optional<int> averagingWindowDays;
        LineAppearance appearance;
        // Keyed by GraphSeriesDescriptor::key.
        std::map<std::string, SeriesStyleOverride, std::less<>> seriesStyles;
        // Keyed by GraphAxisDescriptor::key; holds every axis the graph offers.
        std::map<std::string, AxisPolicy, std::less<>> axisPolicies;

        bool operator==(const GraphPreferences &) const = default;
    };

    struct GraphSeriesDescriptor {
        std::string_view key;
        std::string_view axisKey;
        // Absent: drawn in the theme's foreground colour.
        std::optional<RgbaColor> defaultColor;
    };

    struct GraphAxisDescriptor {
        std::string_view key;
        AxisPolicy defaultPolicy;
    };

    struct GraphDescriptor {
        GraphKey key;
        bool dateRange = false;
        bool latestRuns = false;
        bool averagingWindow = false;
        bool defaultLineModeMarkers = false;
        std::vector<GraphSeriesDescriptor> series;
        std::vector<GraphAxisDescriptor> axes;
    };

    [[nodiscard]] const GraphDescriptor &graphDescriptor(GraphKey key);

    inline constexpr int kDefaultAveragingWindowDays = 3;
    // Equal to SeriesPainter's default line width.
    inline constexpr double kDefaultSeriesWidth = 3.0;

    struct SeriesStyleDefault {
        std::optional<RgbaColor> color;
        double width = kDefaultSeriesWidth;
    };

    [[nodiscard]] SeriesStyleDefault defaultSeriesStyle(GraphKey graph, std::string_view seriesKey);

    [[nodiscard]] GraphPreferences defaultGraphPreferences(GraphKey key);

    // ── Whole snapshot ───────────────────────────────────────────────────────────────────────────
    // The run graph's Graph Lines document. A null allocator means its ID space is exhausted.
    struct RunSeriesDocument {
        std::vector<SeriesConfig> series;
        std::optional<SeriesId> nextSeriesId;
        std::vector<AxisConfig> axes;
        std::optional<AxisId> nextAxisId;
    };

    [[nodiscard]] bool operator==(const RunSeriesDocument &left, const RunSeriesDocument &right);

    [[nodiscard]] RunSeriesDocument defaultRunSeriesDocument();

    struct GraphConfiguration {
        std::array<GraphPreferences, kGraphKeys.size()> graphs;
        RunSeriesDocument runSeries;
        // The run graph's Y-axis-label series; absent means the first visible one.
        std::optional<SeriesId> runAxisLabelSeriesId;

        [[nodiscard]] GraphPreferences &preferences(const GraphKey key) {
            return graphs[static_cast<std::size_t>(key)];
        }

        [[nodiscard]] const GraphPreferences &preferences(const GraphKey key) const {
            return graphs[static_cast<std::size_t>(key)];
        }

        bool operator==(const GraphConfiguration &) const = default;
    };

    [[nodiscard]] GraphConfiguration defaultGraphConfiguration();

    // ── Validation ───────────────────────────────────────────────────────────────────────────────
    enum class GraphConfigurationIssueCode {
        NotApplicable,
        MissingValue,
        InvalidDate,
        ReversedDateRange,
        NonPositiveCount,
        CountTooLarge,
        NonFiniteNumber,
        LineWidthOutOfRange,
        AxisBoundOutOfRange,
        FixedBoundsNotIncreasing,
        UnknownSeries,
        UnknownAxis,
        InvalidRunSeries
    };

    // group: "dataRange", "axes", "seriesStyle" or "graphLines".
    // target: a series or axis key; for "graphLines", a path such as "records[2].yAxisId"; empty for
    // whole-graph fields.
    // field: "range", "recentDays", "latestRuns", "startDate", "endDate", "averagingWindow", "width",
    // "fitMode", "fixedMin" or "fixedMax"; empty when the target itself is the problem.
    struct GraphConfigurationIssue {
        GraphConfigurationIssueCode code;
        GraphKey graph;
        std::string group;
        std::string target;
        std::string field;

        bool operator==(const GraphConfigurationIssue &) const = default;
    };

    [[nodiscard]] std::vector<GraphConfigurationIssue> validateGraphPreferences(
        GraphKey key, const GraphPreferences &preferences);

    [[nodiscard]] std::vector<GraphConfigurationIssue> validateRunSeriesDocument(const RunSeriesDocument &document);

    [[nodiscard]] std::vector<GraphConfigurationIssue> validateGraphConfiguration(
        const GraphConfiguration &configuration);

    // ── Committed state ──────────────────────────────────────────────────────────────────────────
    // Immutable once published: a replacement installs a new configuration rather than editing this one.
    struct GraphConfigurationSnapshot {
        std::shared_ptr<const GraphConfiguration> configuration;
        std::uint64_t revision = 0;
    };

    // Which parts of one graph's preferences a committed change touched. A consumer re-derives only
    // what its sections name: `style` never moves a measurement, `range` never moves the averaging.
    struct GraphSectionChange {
        // The range preference, anchor flag included.
        bool range = false;
        bool window = false;
        // Appearance, series overrides and axis policies.
        bool style = false;

        [[nodiscard]] bool any() const { return range || window || style; }

        bool operator==(const GraphSectionChange &) const = default;
    };

    struct GraphConfigurationChange {
        std::uint64_t revision = 0;
        // Graphs whose GraphPreferences changed, in kGraphKeys order.
        std::vector<GraphKey> graphs;
        bool runSeries = false;
        bool runAxisLabel = false;
        // Indexed by GraphKey; every entry of a graph missing from `graphs` is empty.
        std::array<GraphSectionChange, kGraphKeys.size()> sections;

        [[nodiscard]] const GraphSectionChange &sectionsOf(const GraphKey key) const {
            return sections[static_cast<std::size_t>(key)];
        }

        [[nodiscard]] bool empty() const { return graphs.empty() && !runSeries && !runAxisLabel; }

        bool operator==(const GraphConfigurationChange &) const = default;
    };

    // The revision is left 0 for the publisher to stamp.
    [[nodiscard]] GraphConfigurationChange diffGraphConfigurations(const GraphConfiguration &before,
                                                                   const GraphConfiguration &after);

    struct GraphConfigurationSaveResult {
        // Empty when accepted; otherwise nothing was replaced, written or published.
        std::vector<GraphConfigurationIssue> issues;
        // The committed revision once the call returns.
        std::uint64_t revision = 0;

        [[nodiscard]] bool accepted() const { return issues.empty(); }
    };
}
