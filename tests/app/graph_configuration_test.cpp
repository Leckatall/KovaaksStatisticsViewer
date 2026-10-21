#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>

#include "app/contracts/graph_configuration.h"
#include "app/contracts/graph_presentation.h"

using namespace ksv::application;
using namespace std::chrono;

namespace {
    bool hasIssue(const std::vector<GraphConfigurationIssue> &issues, const GraphConfigurationIssueCode code,
                  const GraphKey graph, const std::string_view group, const std::string_view target,
                  const std::string_view field) {
        return std::ranges::any_of(issues, [&](const GraphConfigurationIssue &issue) {
            return issue.code == code && issue.graph == graph && issue.group == group && issue.target == target &&
                   issue.field == field;
        });
    }

    GraphPreferences withRange(const GraphKey graph, GraphRange range) {
        auto preferences = defaultGraphPreferences(graph);
        preferences.range->range = std::move(range);
        return preferences;
    }

    constexpr year_month_day date(const int y, const unsigned m, const unsigned d) {
        return year_month_day{year{y}, month{m}, day{d}};
    }

    TEST(GraphKeys, RoundTripThroughTheirStableNames) {
        EXPECT_EQ(graphKeyName(GraphKey::Run), "run");
        EXPECT_EQ(graphKeyName(GraphKey::DailyPlaytime), "dailyPlaytime");
        EXPECT_EQ(graphKeyName(GraphKey::ScenarioHistory), "scenarioHistory");
        EXPECT_EQ(graphKeyName(GraphKey::BenchmarkRank), "benchmarkRank");
        EXPECT_EQ(graphKeyName(GraphKey::BenchmarkPlaytime), "benchmarkPlaytime");
        for (const auto key: kGraphKeys) EXPECT_EQ(graphKeyFromName(graphKeyName(key)), key);
        EXPECT_FALSE(graphKeyFromName("Run"));
        EXPECT_FALSE(graphKeyFromName(""));
    }

    TEST(GraphConfigurationDefaults, EachGraphOpensOnItsOwnRecentRange) {
        const auto range = [](const GraphKey key) {
            const auto &preferences = defaultGraphPreferences(key);
            EXPECT_TRUE(preferences.range) << graphKeyName(key);
            return preferences.range.value_or(RangePreference{});
        };
        EXPECT_FALSE(defaultGraphPreferences(GraphKey::Run).range);
        EXPECT_EQ(range(GraphKey::DailyPlaytime), (RangePreference{RecentDays{30}, true}));
        EXPECT_EQ(range(GraphKey::ScenarioHistory), (RangePreference{LatestRuns{100}, false}));
        EXPECT_EQ(range(GraphKey::BenchmarkRank), (RangePreference{RecentDays{90}, true}));
        EXPECT_EQ(range(GraphKey::BenchmarkPlaytime), (RangePreference{RecentDays{30}, true}));
        for (const auto key: kGraphKeys)
            EXPECT_TRUE(validateGraphPreferences(key, defaultGraphPreferences(key)).empty()) << graphKeyName(key);
    }

    TEST(GraphConfigurationDefaults, WindowsMarkersAndAxesKeepTheirDefaults) {
        const auto configuration = defaultGraphConfiguration();
        for (const auto key: kGraphKeys) {
            const auto &preferences = configuration.preferences(key);
            EXPECT_EQ(preferences.appearance.connection, ConnectionMode::Smooth) << graphKeyName(key);
            EXPECT_TRUE(preferences.seriesStyles.empty()) << graphKeyName(key);
        }
        EXPECT_TRUE(configuration.preferences(GraphKey::ScenarioHistory).appearance.lineModeMarkers);
        EXPECT_FALSE(configuration.preferences(GraphKey::Run).appearance.lineModeMarkers);
        EXPECT_FALSE(configuration.preferences(GraphKey::DailyPlaytime).appearance.lineModeMarkers);
        EXPECT_FALSE(configuration.preferences(GraphKey::BenchmarkRank).appearance.lineModeMarkers);
        EXPECT_FALSE(configuration.preferences(GraphKey::BenchmarkPlaytime).appearance.lineModeMarkers);

        EXPECT_EQ(configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays, 7);
        EXPECT_EQ(configuration.preferences(GraphKey::BenchmarkPlaytime).averagingWindowDays, 7);
        EXPECT_FALSE(configuration.preferences(GraphKey::Run).averagingWindowDays);
        EXPECT_FALSE(configuration.preferences(GraphKey::ScenarioHistory).averagingWindowDays);
        EXPECT_FALSE(configuration.preferences(GraphKey::BenchmarkRank).averagingWindowDays);

        const auto &history = configuration.preferences(GraphKey::ScenarioHistory).axisPolicies;
        ASSERT_EQ(history.size(), 3U);
        EXPECT_EQ(history.at("score"), AxisPolicy{FitVisibleData{}});
        EXPECT_EQ(history.at("accuracy"), AxisPolicy{FitVisibleData{}});
        EXPECT_EQ(history.at("counts"), AxisPolicy{StartAtZero{}});
        EXPECT_EQ(configuration.preferences(GraphKey::DailyPlaytime).axisPolicies.at("playtime"),
                  AxisPolicy{StartAtZero{}});
        EXPECT_EQ(configuration.preferences(GraphKey::BenchmarkPlaytime).axisPolicies.at("playtime"),
                  AxisPolicy{StartAtZero{}});
        EXPECT_TRUE(configuration.preferences(GraphKey::BenchmarkRank).axisPolicies.empty());
        EXPECT_TRUE(configuration.preferences(GraphKey::Run).axisPolicies.empty());

        EXPECT_FALSE(configuration.runAxisLabelSeriesId);
        EXPECT_TRUE(sameSeriesConfigs(configuration.runSeries.series, defaultSeriesConfigs()));
        EXPECT_TRUE(sameAxisConfigs(configuration.runSeries.axes, defaultAxisConfigs()));
        EXPECT_EQ(configuration.runSeries.nextSeriesId, SeriesId{9});
        EXPECT_EQ(configuration.runSeries.nextAxisId, AxisId{3});
        EXPECT_TRUE(validateGraphConfiguration(configuration).empty());
    }

    TEST(GraphConfigurationDefaults, DefaultRunSeriesDocumentDerivesItsAllocatorsFromTheDefaultLists) {
        const auto document = defaultRunSeriesDocument();
        EXPECT_EQ(document.nextSeriesId, SeriesId{9});
        EXPECT_EQ(document.nextAxisId, AxisId{3});
        EXPECT_TRUE(validateRunSeriesDocument(document).empty());
    }

    TEST(GraphConfigurationDefaults, SeriesStylesMatchTheExistingGraphColoursAndPainterWidth) {
        const auto colour = [](const GraphKey graph, const std::string_view series) {
            return defaultSeriesStyle(graph, series).color;
        };
        EXPECT_EQ(colour(GraphKey::ScenarioHistory, "score"), (RgbaColor{0, 150, 0, 255}));
        EXPECT_EQ(colour(GraphKey::ScenarioHistory, "accuracy"), (RgbaColor{0, 255, 255, 255}));
        EXPECT_EQ(colour(GraphKey::ScenarioHistory, "shots"), (RgbaColor{255, 165, 0, 255}));
        EXPECT_EQ(colour(GraphKey::ScenarioHistory, "hits"), (RgbaColor{124, 179, 66, 255}));
        EXPECT_EQ(colour(GraphKey::ScenarioHistory, "misses"), (RgbaColor{229, 57, 53, 255}));
        EXPECT_EQ(colour(GraphKey::DailyPlaytime, "playtime"), (RgbaColor{77, 208, 225, 255}));
        EXPECT_EQ(colour(GraphKey::BenchmarkPlaytime, "playtime"), (RgbaColor{77, 208, 225, 255}));
        // The rank line is drawn in the theme's foreground colour, not a fixed one.
        EXPECT_FALSE(colour(GraphKey::BenchmarkRank, "rank"));
        for (const auto key: kGraphKeys)
            for (const auto &series: graphDescriptor(key).series)
                EXPECT_EQ(defaultSeriesStyle(key, series.key).width, kDefaultSeriesWidth);
        EXPECT_EQ(kDefaultSeriesWidth, 3.0);
    }

    TEST(GraphDescriptors, NameTheStableSeriesAndAxisKeysOfEachGraph) {
        const auto seriesKeys = [](const GraphKey graph) {
            std::vector<std::string> keys;
            for (const auto &series: graphDescriptor(graph).series) keys.emplace_back(series.key);
            return keys;
        };
        const auto axisKeys = [](const GraphKey graph) {
            std::vector<std::string> keys;
            for (const auto &axis: graphDescriptor(graph).axes) keys.emplace_back(axis.key);
            return keys;
        };
        EXPECT_TRUE(seriesKeys(GraphKey::Run).empty());
        EXPECT_EQ(seriesKeys(GraphKey::ScenarioHistory),
                  (std::vector<std::string>{"score", "accuracy", "shots", "hits", "misses", "scoreAverage"}));
        EXPECT_EQ(seriesKeys(GraphKey::DailyPlaytime), (std::vector<std::string>{"playtime"}));
        EXPECT_EQ(seriesKeys(GraphKey::BenchmarkRank), (std::vector<std::string>{"rank"}));
        EXPECT_EQ(seriesKeys(GraphKey::BenchmarkPlaytime), (std::vector<std::string>{"playtime"}));

        EXPECT_EQ(axisKeys(GraphKey::ScenarioHistory), (std::vector<std::string>{"score", "accuracy", "counts"}));
        EXPECT_EQ(axisKeys(GraphKey::DailyPlaytime), (std::vector<std::string>{"playtime"}));
        EXPECT_EQ(axisKeys(GraphKey::BenchmarkPlaytime), (std::vector<std::string>{"playtime"}));
        EXPECT_TRUE(axisKeys(GraphKey::BenchmarkRank).empty());
        EXPECT_TRUE(axisKeys(GraphKey::Run).empty());

        const auto &history = graphDescriptor(GraphKey::ScenarioHistory).series;
        for (const auto &series: history) {
            if (series.key == "shots" || series.key == "hits" || series.key == "misses")
                EXPECT_EQ(series.axisKey, "counts");
            else if (series.key == "scoreAverage")
                EXPECT_EQ(series.axisKey, "score");
            else
                EXPECT_EQ(series.axisKey, series.key);
        }

        EXPECT_FALSE(graphDescriptor(GraphKey::Run).dateRange);
        EXPECT_TRUE(graphDescriptor(GraphKey::ScenarioHistory).latestRuns);
        EXPECT_FALSE(graphDescriptor(GraphKey::DailyPlaytime).latestRuns);
        EXPECT_FALSE(graphDescriptor(GraphKey::BenchmarkRank).latestRuns);
        EXPECT_TRUE(graphDescriptor(GraphKey::DailyPlaytime).averagingWindow);
        EXPECT_TRUE(graphDescriptor(GraphKey::BenchmarkPlaytime).averagingWindow);
        EXPECT_FALSE(graphDescriptor(GraphKey::ScenarioHistory).averagingWindow);
    }

    TEST(GraphConfigurationValidation, AcceptsEveryValidRangeKindWithinItsStatedLimits) {
        for (const auto key: {GraphKey::DailyPlaytime, GraphKey::ScenarioHistory, GraphKey::BenchmarkRank,
                              GraphKey::BenchmarkPlaytime}) {
            EXPECT_TRUE(validateGraphPreferences(key, withRange(key, RecentDays{1})).empty());
            EXPECT_TRUE(validateGraphPreferences(key, withRange(key, RecentDays{kMaximumRangeDays})).empty());
            EXPECT_TRUE(validateGraphPreferences(key, withRange(key, CustomDates{date(2026, 3, 5), date(2026, 3, 5)}))
                .empty());
            EXPECT_TRUE(validateGraphPreferences(
                key, withRange(key, CustomDates{date(1970, 1, 1), date(9999, 12, 31)})).empty());
        }
        EXPECT_TRUE(validateGraphPreferences(GraphKey::ScenarioHistory,
                                             withRange(GraphKey::ScenarioHistory, LatestRuns{1})).empty());
        EXPECT_TRUE(validateGraphPreferences(GraphKey::ScenarioHistory,
                                             withRange(GraphKey::ScenarioHistory, LatestRuns{kMaximumLatestRuns})).
            empty());
    }

    TEST(GraphConfigurationValidation, RejectsReversedAndInvalidCustomDates) {
        const auto key = GraphKey::DailyPlaytime;
        const auto reversed = validateGraphPreferences(key, withRange(key, CustomDates{date(2026, 3, 6), date(2026, 3, 5)}));
        EXPECT_TRUE(hasIssue(reversed, GraphConfigurationIssueCode::ReversedDateRange, key, "dataRange", "", "endDate"));

        const auto impossible = validateGraphPreferences(key, withRange(key, CustomDates{date(2026, 2, 30), date(2026, 3, 1)}));
        EXPECT_TRUE(hasIssue(impossible, GraphConfigurationIssueCode::InvalidDate, key, "dataRange", "", "startDate"));

        const auto impossibleEnd = validateGraphPreferences(key, withRange(key, CustomDates{date(2026, 2, 1), date(2026, 13, 1)}));
        EXPECT_TRUE(hasIssue(impossibleEnd, GraphConfigurationIssueCode::InvalidDate, key, "dataRange", "", "endDate"));

        const auto beforeEpoch = validateGraphPreferences(key, withRange(key, CustomDates{date(1969, 12, 31), date(2026, 3, 1)}));
        EXPECT_TRUE(hasIssue(beforeEpoch, GraphConfigurationIssueCode::InvalidDate, key, "dataRange", "", "startDate"));

        const auto afterLimit = validateGraphPreferences(key, withRange(key, CustomDates{date(2026, 1, 1), date(10000, 1, 1)}));
        EXPECT_TRUE(hasIssue(afterLimit, GraphConfigurationIssueCode::InvalidDate, key, "dataRange", "", "endDate"));
    }

    TEST(GraphConfigurationValidation, RejectsNonPositiveAndExcessiveCountsAndWindows) {
        for (const int days: {0, -1}) {
            const auto issues = validateGraphPreferences(GraphKey::BenchmarkRank,
                                                         withRange(GraphKey::BenchmarkRank, RecentDays{days}));
            EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::NonPositiveCount, GraphKey::BenchmarkRank,
                "dataRange", "", "recentDays"));
        }
        EXPECT_TRUE(hasIssue(validateGraphPreferences(GraphKey::BenchmarkRank,
                withRange(GraphKey::BenchmarkRank, RecentDays{kMaximumRangeDays + 1})),
            GraphConfigurationIssueCode::CountTooLarge, GraphKey::BenchmarkRank, "dataRange", "", "recentDays"));

        for (const int count: {0, -5}) {
            const auto issues = validateGraphPreferences(GraphKey::ScenarioHistory,
                                                         withRange(GraphKey::ScenarioHistory, LatestRuns{count}));
            EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::NonPositiveCount, GraphKey::ScenarioHistory,
                "dataRange", "", "latestRuns"));
        }
        EXPECT_TRUE(hasIssue(validateGraphPreferences(GraphKey::ScenarioHistory,
                withRange(GraphKey::ScenarioHistory, LatestRuns{kMaximumLatestRuns + 1})),
            GraphConfigurationIssueCode::CountTooLarge, GraphKey::ScenarioHistory, "dataRange", "", "latestRuns"));

        for (const auto key: {GraphKey::DailyPlaytime, GraphKey::BenchmarkPlaytime}) {
            for (const int window: {0, -3}) {
                auto preferences = defaultGraphPreferences(key);
                preferences.averagingWindowDays = window;
                EXPECT_TRUE(hasIssue(validateGraphPreferences(key, preferences),
                    GraphConfigurationIssueCode::NonPositiveCount, key, "dataRange", "", "averagingWindow"));
            }
            auto preferences = defaultGraphPreferences(key);
            preferences.averagingWindowDays = kMaximumAveragingWindowDays;
            EXPECT_TRUE(validateGraphPreferences(key, preferences).empty());
            preferences.averagingWindowDays = kMaximumAveragingWindowDays + 1;
            EXPECT_TRUE(hasIssue(validateGraphPreferences(key, preferences),
                GraphConfigurationIssueCode::CountTooLarge, key, "dataRange", "", "averagingWindow"));
        }
    }

    TEST(GraphConfigurationValidation, RejectsFieldsOnGraphsThatDoNotOfferThem) {
        EXPECT_TRUE(hasIssue(validateGraphPreferences(GraphKey::DailyPlaytime,
                withRange(GraphKey::DailyPlaytime, LatestRuns{5})),
            GraphConfigurationIssueCode::NotApplicable, GraphKey::DailyPlaytime, "dataRange", "", "latestRuns"));

        auto run = defaultGraphPreferences(GraphKey::Run);
        run.range = RangePreference{};
        run.averagingWindowDays = 3;
        run.seriesStyles["score"] = SeriesStyleOverride{RgbaColor{1, 2, 3, 255}, std::nullopt};
        run.axisPolicies["score"] = FitVisibleData{};
        const auto runIssues = validateGraphPreferences(GraphKey::Run, run);
        EXPECT_TRUE(hasIssue(runIssues, GraphConfigurationIssueCode::NotApplicable, GraphKey::Run, "dataRange", "", "range"));
        EXPECT_TRUE(hasIssue(runIssues, GraphConfigurationIssueCode::NotApplicable, GraphKey::Run, "dataRange", "",
            "averagingWindow"));
        EXPECT_TRUE(hasIssue(runIssues, GraphConfigurationIssueCode::UnknownSeries, GraphKey::Run, "seriesStyle", "score", ""));
        EXPECT_TRUE(hasIssue(runIssues, GraphConfigurationIssueCode::UnknownAxis, GraphKey::Run, "axes", "score", ""));

        auto history = defaultGraphPreferences(GraphKey::ScenarioHistory);
        history.averagingWindowDays = 3;
        EXPECT_TRUE(hasIssue(validateGraphPreferences(GraphKey::ScenarioHistory, history),
            GraphConfigurationIssueCode::NotApplicable, GraphKey::ScenarioHistory, "dataRange", "", "averagingWindow"));

        auto rank = defaultGraphPreferences(GraphKey::BenchmarkRank);
        rank.axisPolicies["rank"] = FixedBounds{0.0, 3.0};
        EXPECT_TRUE(hasIssue(validateGraphPreferences(GraphKey::BenchmarkRank, rank),
            GraphConfigurationIssueCode::UnknownAxis, GraphKey::BenchmarkRank, "axes", "rank", ""));
    }

    TEST(GraphConfigurationValidation, RequiresTheFieldsAGraphOffers) {
        auto playtime = defaultGraphPreferences(GraphKey::DailyPlaytime);
        playtime.range.reset();
        playtime.averagingWindowDays.reset();
        playtime.axisPolicies.clear();
        const auto issues = validateGraphPreferences(GraphKey::DailyPlaytime, playtime);
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::MissingValue, GraphKey::DailyPlaytime, "dataRange", "", "range"));
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::MissingValue, GraphKey::DailyPlaytime, "dataRange", "",
            "averagingWindow"));
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::MissingValue, GraphKey::DailyPlaytime, "axes", "playtime",
            "fitMode"));
    }

    TEST(GraphConfigurationValidation, RejectsInvalidSeriesWidthsAndUnknownSeries) {
        const auto key = GraphKey::ScenarioHistory;
        for (const double width: {kMinimumLineWidth, 3.0, kMaximumLineWidth}) {
            auto preferences = defaultGraphPreferences(key);
            preferences.seriesStyles["hits"] = SeriesStyleOverride{std::nullopt, width};
            EXPECT_TRUE(validateGraphPreferences(key, preferences).empty()) << width;
        }
        for (const double width: {0.0, -1.0, kMinimumLineWidth - 0.01, kMaximumLineWidth + 0.01}) {
            auto preferences = defaultGraphPreferences(key);
            preferences.seriesStyles["hits"] = SeriesStyleOverride{std::nullopt, width};
            EXPECT_TRUE(hasIssue(validateGraphPreferences(key, preferences),
                GraphConfigurationIssueCode::LineWidthOutOfRange, key, "seriesStyle", "hits", "width")) << width;
        }
        for (const double width: {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
            auto preferences = defaultGraphPreferences(key);
            preferences.seriesStyles["hits"] = SeriesStyleOverride{std::nullopt, width};
            EXPECT_TRUE(hasIssue(validateGraphPreferences(key, preferences),
                GraphConfigurationIssueCode::NonFiniteNumber, key, "seriesStyle", "hits", "width"));
        }
        auto preferences = defaultGraphPreferences(key);
        preferences.seriesStyles["kills"] = SeriesStyleOverride{RgbaColor{1, 2, 3, 255}, std::nullopt};
        EXPECT_TRUE(hasIssue(validateGraphPreferences(key, preferences),
            GraphConfigurationIssueCode::UnknownSeries, key, "seriesStyle", "kills", ""));
    }

    TEST(GraphConfigurationValidation, RejectsNonFiniteOutOfRangeAndNonIncreasingFixedBounds) {
        const auto key = GraphKey::ScenarioHistory;
        const auto withPolicy = [&](AxisPolicy policy) {
            auto preferences = defaultGraphPreferences(key);
            preferences.axisPolicies["accuracy"] = std::move(policy);
            return validateGraphPreferences(key, preferences);
        };
        EXPECT_TRUE(withPolicy(FixedBounds{-12.5, 87.25}).empty());
        EXPECT_TRUE(withPolicy(FixedBounds{-kMaximumAxisBoundMagnitude, kMaximumAxisBoundMagnitude}).empty());

        EXPECT_TRUE(hasIssue(withPolicy(FixedBounds{50.0, 50.0}), GraphConfigurationIssueCode::FixedBoundsNotIncreasing,
            key, "axes", "accuracy", "fixedMax"));
        EXPECT_TRUE(hasIssue(withPolicy(FixedBounds{60.0, 50.0}), GraphConfigurationIssueCode::FixedBoundsNotIncreasing,
            key, "axes", "accuracy", "fixedMax"));

        constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
        constexpr auto infinity = std::numeric_limits<double>::infinity();
        EXPECT_TRUE(hasIssue(withPolicy(FixedBounds{nan, 1.0}), GraphConfigurationIssueCode::NonFiniteNumber,
            key, "axes", "accuracy", "fixedMin"));
        EXPECT_TRUE(hasIssue(withPolicy(FixedBounds{0.0, infinity}), GraphConfigurationIssueCode::NonFiniteNumber,
            key, "axes", "accuracy", "fixedMax"));
        EXPECT_TRUE(hasIssue(withPolicy(FixedBounds{-infinity, 1.0}), GraphConfigurationIssueCode::NonFiniteNumber,
            key, "axes", "accuracy", "fixedMin"));
        EXPECT_TRUE(hasIssue(withPolicy(FixedBounds{-kMaximumAxisBoundMagnitude * 10, 1.0}),
            GraphConfigurationIssueCode::AxisBoundOutOfRange, key, "axes", "accuracy", "fixedMin"));
        EXPECT_TRUE(hasIssue(withPolicy(FixedBounds{0.0, kMaximumAxisBoundMagnitude * 10}),
            GraphConfigurationIssueCode::AxisBoundOutOfRange, key, "axes", "accuracy", "fixedMax"));
    }

    // EDGE-02/STATE-03: Save must see an invalid field even when it belongs to a graph the user is not
    // currently looking at.
    TEST(GraphConfigurationValidation, WholeSnapshotValidationReportsEveryGraphsIssues) {
        auto configuration = defaultGraphConfiguration();
        configuration.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 0;
        configuration.preferences(GraphKey::BenchmarkRank).range->range = RecentDays{0};
        const auto issues = validateGraphConfiguration(configuration);
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::NonPositiveCount, GraphKey::DailyPlaytime,
            "dataRange", "", "averagingWindow"));
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::NonPositiveCount, GraphKey::BenchmarkRank,
            "dataRange", "", "recentDays"));
    }

    TEST(GraphConfigurationValidation, ReportsInvalidRunSeriesDocumentsUnderGraphLines) {
        auto configuration = defaultGraphConfiguration();
        configuration.runSeries.series[0].presentation.lineStyle.width = 100.0;
        configuration.runSeries.series[1].yAxisId = AxisId{99};
        configuration.runSeries.nextSeriesId = SeriesId{8};
        configuration.runSeries.axes.push_back(configuration.runSeries.axes[0]);
        configuration.runSeries.nextAxisId = AxisId{2};
        const auto issues = validateGraphConfiguration(configuration);
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::InvalidRunSeries, GraphKey::Run, "graphLines",
            "records[0].presentation.lineStyle.width", ""));
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::InvalidRunSeries, GraphKey::Run, "graphLines",
            "records[1].yAxisId", ""));
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::InvalidRunSeries, GraphKey::Run, "graphLines",
            "nextComputedSeriesId", ""));
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::InvalidRunSeries, GraphKey::Run, "graphLines",
            "axes[2].id", ""));
        EXPECT_TRUE(hasIssue(issues, GraphConfigurationIssueCode::InvalidRunSeries, GraphKey::Run, "graphLines",
            "nextAxisId", ""));
    }

    TEST(GraphConfigurationValidation, ExhaustedAllocatorsAreValidOnlyOnceTheMaximumIdIsTaken) {
        auto configuration = defaultGraphConfiguration();
        configuration.runSeries.nextSeriesId.reset();
        configuration.runSeries.nextAxisId.reset();
        const auto exhaustedEarly = validateGraphConfiguration(configuration);
        EXPECT_TRUE(hasIssue(exhaustedEarly, GraphConfigurationIssueCode::InvalidRunSeries, GraphKey::Run, "graphLines",
            "nextComputedSeriesId", ""));
        EXPECT_TRUE(hasIssue(exhaustedEarly, GraphConfigurationIssueCode::InvalidRunSeries, GraphKey::Run, "graphLines",
            "nextAxisId", ""));

        auto maximal = configuration.runSeries.series.back();
        maximal.id = SeriesId{std::numeric_limits<uint64_t>::max()};
        maximal.presentation.displayPosition = static_cast<uint32_t>(configuration.runSeries.series.size());
        configuration.runSeries.series.push_back(maximal);
        configuration.runSeries.axes.push_back(AxisConfig{AxisId{std::numeric_limits<uint64_t>::max()}, "Last"});
        EXPECT_TRUE(validateGraphConfiguration(configuration).empty());
    }

    TEST(GraphConfigurationEquality, ComparesExpressionsByValueAndEveryPreference) {
        auto left = defaultGraphConfiguration();
        auto right = defaultGraphConfiguration();
        EXPECT_EQ(left, right);
        right.preferences(GraphKey::ScenarioHistory).appearance.lineModeMarkers = false;
        EXPECT_NE(left, right);
        right = defaultGraphConfiguration();
        right.runSeries.series[0].expression = primitive(PrimitiveMetric::Hits);
        EXPECT_NE(left, right);
        right = defaultGraphConfiguration();
        right.runSeries.axes[0].options.fallbackSpan = 2.0;
        EXPECT_NE(left, right);
        right = defaultGraphConfiguration();
        right.runAxisLabelSeriesId = SeriesId{1};
        EXPECT_NE(left, right);
    }

    TEST(GraphConfigurationDiff, NamesTheSectionsOfEachChangedGraph) {
        const auto before = defaultGraphConfiguration();
        const auto sections = [&](const GraphConfiguration &after, const GraphKey graph) {
            return diffGraphConfigurations(before, after).sectionsOf(graph);
        };

        auto range = before;
        range.preferences(GraphKey::ScenarioHistory).range = RangePreference{RecentDays{30}, false};
        EXPECT_EQ(sections(range, GraphKey::ScenarioHistory), (GraphSectionChange{.range = true}));

        auto anchor = before;
        anchor.preferences(GraphKey::BenchmarkRank).range->anchorToLatestRun = false;
        EXPECT_EQ(sections(anchor, GraphKey::BenchmarkRank), (GraphSectionChange{.range = true}));

        auto window = before;
        window.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 5;
        EXPECT_EQ(sections(window, GraphKey::DailyPlaytime), (GraphSectionChange{.window = true}));

        auto connection = before;
        connection.preferences(GraphKey::Run).appearance.connection = ConnectionMode::PointsOnly;
        EXPECT_EQ(sections(connection, GraphKey::Run), (GraphSectionChange{.style = true}));

        auto markers = before;
        markers.preferences(GraphKey::ScenarioHistory).appearance.lineModeMarkers = false;
        EXPECT_EQ(sections(markers, GraphKey::ScenarioHistory), (GraphSectionChange{.style = true}));

        auto colour = before;
        colour.preferences(GraphKey::BenchmarkRank).seriesStyles["rank"].color = RgbaColor{1, 2, 3, 255};
        EXPECT_EQ(sections(colour, GraphKey::BenchmarkRank), (GraphSectionChange{.style = true}));

        auto policy = before;
        policy.preferences(GraphKey::BenchmarkPlaytime).axisPolicies["playtime"] = FitVisibleData{};
        EXPECT_EQ(sections(policy, GraphKey::BenchmarkPlaytime), (GraphSectionChange{.style = true}));
    }

    TEST(GraphConfigurationDiff, SeveralSectionsOfOneGraphAndOtherGraphsStayIndependent) {
        const auto before = defaultGraphConfiguration();
        auto after = before;
        after.preferences(GraphKey::BenchmarkPlaytime).averagingWindowDays = 5;
        after.preferences(GraphKey::BenchmarkPlaytime).appearance.connection = ConnectionMode::Straight;
        after.preferences(GraphKey::DailyPlaytime).range = RangePreference{RecentDays{7}, true};

        const auto change = diffGraphConfigurations(before, after);

        EXPECT_EQ(change.sectionsOf(GraphKey::BenchmarkPlaytime), (GraphSectionChange{.window = true, .style = true}));
        EXPECT_EQ(change.sectionsOf(GraphKey::DailyPlaytime), (GraphSectionChange{.range = true}));
        for (const auto graph: {GraphKey::Run, GraphKey::ScenarioHistory, GraphKey::BenchmarkRank})
            EXPECT_FALSE(change.sectionsOf(graph).any()) << graphKeyName(graph);
    }

    TEST(GraphPresentation, DefaultsReproduceTodaysLinesMarkersAndColours) {
        const auto history = defaultGraphPresentation(GraphKey::ScenarioHistory);
        EXPECT_EQ(history.appearance, (LineAppearance{ConnectionMode::Smooth, true}));
        EXPECT_EQ(history.seriesStyle("score"), (SeriesStyle{RgbaColor{0, 150, 0, 255}, 3.0}));
        EXPECT_EQ(history.seriesStyle("misses").color, (RgbaColor{229, 57, 53, 255}));
        EXPECT_EQ(history.axisPolicies.at("counts"), AxisPolicy{StartAtZero{}});
        for (const auto graph: {GraphKey::Run, GraphKey::DailyPlaytime, GraphKey::BenchmarkRank,
                                GraphKey::BenchmarkPlaytime})
            EXPECT_EQ(defaultGraphPresentation(graph).appearance, (LineAppearance{ConnectionMode::Smooth, false}))
                << graphKeyName(graph);
        EXPECT_FALSE(defaultGraphPresentation(GraphKey::BenchmarkRank).seriesStyle("rank").color);
    }

    TEST(GraphPresentation, OverridesLayerOverTheDefaultOneFieldAtATime) {
        auto preferences = defaultGraphPreferences(GraphKey::ScenarioHistory);
        preferences.appearance = {ConnectionMode::PointsOnly, false};
        preferences.seriesStyles["score"] = {RgbaColor{9, 8, 7, 255}, std::nullopt};
        preferences.seriesStyles["hits"] = {std::nullopt, 6.5};

        const auto presentation = presentationOf(GraphKey::ScenarioHistory, preferences);

        EXPECT_EQ(presentation.appearance, (LineAppearance{ConnectionMode::PointsOnly, false}));
        EXPECT_EQ(presentation.seriesStyle("score"), (SeriesStyle{RgbaColor{9, 8, 7, 255}, 3.0}));
        EXPECT_EQ(presentation.seriesStyle("hits"), (SeriesStyle{RgbaColor{124, 179, 66, 255}, 6.5}));
        EXPECT_EQ(presentation.seriesStyle("accuracy"), (SeriesStyle{RgbaColor{0, 255, 255, 255}, 3.0}));
    }

    TEST(GraphPresentation, ARecolouredRankSeriesKeepsItsKeyAndTheDefaultWidth) {
        auto preferences = defaultGraphPreferences(GraphKey::BenchmarkRank);
        preferences.seriesStyles["rank"] = {RgbaColor{200, 0, 0, 255}, std::nullopt};

        EXPECT_EQ(presentationOf(GraphKey::BenchmarkRank, preferences).seriesStyle("rank"),
                  (SeriesStyle{RgbaColor{200, 0, 0, 255}, 3.0}));
    }
}
