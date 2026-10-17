#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "usecases/completion_history_use_case.h"
#include "fake_day_source.h"
#include "fake_graph_configuration_use_case.h"
#include "fake_profile_service.h"
#include "fake_session_controller.h"
#include "run_builders.h"

using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_support;

namespace {
    class CompletionHistoryUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeSessionController> session = std::make_shared<FakeSessionController>();
        std::shared_ptr<FakeProfileService> profile = std::make_shared<FakeProfileService>();
        std::shared_ptr<FakeGraphConfigurationUseCase> configuration =
            std::make_shared<FakeGraphConfigurationUseCase>();
        std::shared_ptr<FakeDaySource> days = std::make_shared<FakeDaySource>();
        CompletionHistoryUseCase use_case{session, profile, configuration, days};

        static constexpr long long kMsPerDay = 86'400'000LL;
        // 2026-10-16, the fake day source's default today.
        static constexpr long long kToday = 20742;

        void setCurrentScenario(const std::string &hash = "scenario-1", const long long start_time = 300) const {
            session->current_run.run_id.scenario_id = {.name = "Scenario One", .hash = hash};
            session->current_run.run_id.start_time = start_time;
        }

        GraphPreferences &preferences() const {
            return configuration->configuration.preferences(GraphKey::ScenarioHistory);
        }

        void setRange(GraphRange range, const bool anchorToLatestRun = false) const {
            preferences().range = RangePreference{std::move(range), anchorToLatestRun};
        }

        void commit(const GraphKey key = GraphKey::ScenarioHistory) const {
            ++configuration->revision;
            configuration->publish({key});
        }

        // One run per entry of `epochDays`, oldest first, for the current scenario.
        void playOnDays(const std::vector<long long> &epochDays, const std::string &hash = "scenario-1") const {
            setCurrentScenario(hash, epochDays.empty() ? 300 : epochDays.back() * kMsPerDay + 7);
            std::vector<RunSummary> runs;
            for (std::size_t i = 0; i < epochDays.size(); ++i)
                runs.push_back({.run_id = {.scenario_id = {.name = "Scenario One", .hash = hash},
                                           .start_time = epochDays[i] * kMsPerDay + 7},
                                .totals = {.score = static_cast<float>(100 + i), .shots = 10, .hits = 5, .misses = 5}});
            profile->completion_history_by_scenario[{.name = "Scenario One", .hash = hash}] = std::move(runs);
        }

        static std::vector<int> runNumbers(const CompletionHistory &history) {
            std::vector<int> numbers;
            for (const auto &row: history.rows) numbers.push_back(row.run_index);
            return numbers;
        }
    };

    TEST_F(CompletionHistoryUseCaseTest, MapsCompletionRowsAndAssignsOneBasedRunIndices) {
        setCurrentScenario();
        profile->completion_history_by_scenario[{.name = "Scenario One", .hash = "scenario-1"}] = {
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100},
             .totals = {.score = 25.0F, .shots = 10, .hits = 4, .misses = 6}},
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 200},
             .totals = {.score = 75.0F, .shots = 20, .hits = 15, .misses = 5}},
        };

        const auto history = use_case.get_history();

        EXPECT_EQ(profile->requested_scenario.hash, "scenario-1");
        EXPECT_EQ(history.scenario_name, "Scenario One");
        ASSERT_EQ(history.rows.size(), 2);
        EXPECT_EQ(history.rows[0].run_index, 1);
        EXPECT_EQ(history.rows[0].start_time_ms, 100);
        EXPECT_DOUBLE_EQ(history.rows[0].score, 25.0);
        EXPECT_DOUBLE_EQ(history.rows[0].accuracy, 0.4);
        EXPECT_DOUBLE_EQ(history.rows[0].shots, 10.0);
        EXPECT_DOUBLE_EQ(history.rows[0].hits, 4.0);
        EXPECT_DOUBLE_EQ(history.rows[0].misses, 6.0);
        EXPECT_EQ(history.rows[1].run_index, 2);
    }

    TEST_F(CompletionHistoryUseCaseTest, AccuracyIsZeroWhenShotsAreZero) {
        setCurrentScenario();
        profile->completion_history_by_scenario[{.name = "Scenario One", .hash = "scenario-1"}] = {
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100},
             .totals = {.shots = 0, .hits = 0}},
        };

        const auto history = use_case.get_history();

        ASSERT_EQ(history.rows.size(), 1);
        EXPECT_DOUBLE_EQ(history.rows[0].accuracy, 0.0);
    }

    TEST_F(CompletionHistoryUseCaseTest, EmptyCurrentScenarioDoesNotQueryProfile) {
        const auto history = use_case.get_history();

        EXPECT_TRUE(history.rows.empty());
        EXPECT_EQ(profile->completion_history_calls, 0);
    }

    TEST_F(CompletionHistoryUseCaseTest, CallbackOnlyFiresWhenScenarioHashChanges) {
        setCurrentScenario("scenario-1", 100);
        int callback_count = 0;
        use_case.onCurrentScenarioChanged([&callback_count] { ++callback_count; });

        session->changeRun("scenario-1", 200);
        EXPECT_EQ(callback_count, 0);

        session->changeRun("scenario-2", 300);
        EXPECT_EQ(callback_count, 1);
    }

    TEST_F(CompletionHistoryUseCaseTest, SelectedRunIndexIsTheCurrentRunsOneBasedRunIndex) {
        setCurrentScenario("scenario-1", 200);
        profile->completion_history_by_scenario[{.name = "Scenario One", .hash = "scenario-1"}] = {
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100}},
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 200}},
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 300}},
        };

        EXPECT_EQ(use_case.selected_run_index(), 2);

        session->current_run.run_id.start_time = 999;
        EXPECT_EQ(use_case.selected_run_index(), std::nullopt) << "a run missing from the history marks nothing";
    }

    TEST_F(CompletionHistoryUseCaseTest, NoSelectedRunHasNoSelectedRunIndex) {
        EXPECT_EQ(use_case.selected_run_index(), std::nullopt);
        EXPECT_EQ(profile->completion_history_calls, 0);
    }

    TEST_F(CompletionHistoryUseCaseTest, SelectedRunCallbackFiresOnEveryRunChange) {
        setCurrentScenario("scenario-1", 100);
        int callback_count = 0;
        use_case.onSelectedRunChanged([&callback_count] { ++callback_count; });

        session->changeRun("scenario-1", 200);
        session->changeRun("scenario-2", 300);

        EXPECT_EQ(callback_count, 2);
    }

    TEST_F(CompletionHistoryUseCaseTest, SelectRunMakesThatHistoryRunCurrent) {
        setCurrentScenario("scenario-1", 300);
        profile->completion_history_by_scenario[{.name = "Scenario One", .hash = "scenario-1"}] = {
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100}},
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 300}},
        };

        use_case.select_run(1);
        ASSERT_TRUE(session->selected_run.has_value());
        EXPECT_EQ(session->selected_run->start_time, 100);
        EXPECT_EQ(session->selected_run->scenario_id.hash, "scenario-1");

        session->selected_run.reset();
        use_case.select_run(0);
        use_case.select_run(3);
        EXPECT_FALSE(session->selected_run.has_value()) << "an index outside the history selects nothing";
    }

    TEST_F(CompletionHistoryUseCaseTest, ProfileChangeAlwaysFiresAndRestoresTheScenarioGate) {
        setCurrentScenario("scenario-1", 100);
        int callback_count = 0;
        use_case.onCurrentScenarioChanged([&callback_count] { ++callback_count; });

        session->notifyProfileChanged();
        EXPECT_EQ(callback_count, 1);

        session->changeRun("scenario-1", 200);
        EXPECT_EQ(callback_count, 1);
    }

    TEST_F(CompletionHistoryUseCaseTest, AllHistoryIsTheDefaultAndFullCountEqualsRowCount) {
        playOnDays({kToday - 30, kToday - 3, kToday});

        const auto history = use_case.get_history();

        EXPECT_EQ(runNumbers(history), (std::vector{1, 2, 3}));
        EXPECT_EQ(history.full_count, 3);
    }

    TEST_F(CompletionHistoryUseCaseTest, LatestRunsKeepTheNewestSuffixWithTheirOriginalRunNumbers) {
        playOnDays({kToday - 40, kToday - 30, kToday - 20, kToday - 10, kToday});
        setRange(LatestRuns{2});

        const auto history = use_case.get_history();

        EXPECT_EQ(runNumbers(history), (std::vector{4, 5}));
        EXPECT_EQ(history.full_count, 5);
        EXPECT_EQ(history.rows[0].start_time_ms, (kToday - 10) * kMsPerDay + 7);
        EXPECT_DOUBLE_EQ(history.rows[0].score, 103.0) << "values come from the original row, not a re-indexed one";
    }

    TEST_F(CompletionHistoryUseCaseTest, LatestRunsLargerThanTheHistoryShowEverything) {
        playOnDays({kToday - 2, kToday});
        setRange(LatestRuns{50});

        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{1, 2}));
    }

    TEST_F(CompletionHistoryUseCaseTest, CustomDatesFilterByRowTimestampNotByRunPosition) {
        playOnDays({kToday - 40, kToday - 30, kToday - 20, kToday - 10, kToday});
        const std::chrono::sys_days first{std::chrono::days{kToday - 30}};
        const std::chrono::sys_days last{std::chrono::days{kToday - 10}};
        setRange(CustomDates{std::chrono::year_month_day{first}, std::chrono::year_month_day{last}});

        const auto history = use_case.get_history();

        EXPECT_EQ(runNumbers(history), (std::vector{2, 3, 4})) << "both endpoints are inclusive";
        EXPECT_EQ(history.full_count, 5);
    }

    TEST_F(CompletionHistoryUseCaseTest, RecentDaysAreCountedBackFromTodayByDefault) {
        playOnDays({kToday - 40, kToday - 6, kToday - 1});
        setRange(RecentDays{7});

        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{2, 3}));
    }

    TEST_F(CompletionHistoryUseCaseTest, RecentDaysCanAnchorToTheCurrentScenarioLatestRun) {
        playOnDays({kToday - 400, kToday - 395, kToday - 392});
        setRange(RecentDays{5}, true);

        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{2, 3}));
    }

    TEST_F(CompletionHistoryUseCaseTest, AnchorIsTheCurrentScenarioRunsNotTheRestOfTheProfile) {
        playOnDays({kToday - 400});
        profile->latest_run = makeRun("other", kToday * kMsPerDay);
        setRange(RecentDays{5}, true);

        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{1}));
    }

    TEST_F(CompletionHistoryUseCaseTest, ANewRunShiftsTheLatestRunsSuffixAndKeepsOriginalRunNumbers) {
        playOnDays({kToday - 30, kToday - 20, kToday - 10});
        setRange(LatestRuns{2});
        const auto before = use_case.get_history();
        ASSERT_EQ(runNumbers(before), (std::vector{2, 3}));

        playOnDays({kToday - 30, kToday - 20, kToday - 10, kToday});
        session->notifyProfileChanged();
        const auto after = use_case.get_history();

        EXPECT_EQ(runNumbers(after), (std::vector{3, 4}));
        EXPECT_EQ(after.full_count, 4);
        EXPECT_EQ(after.rows[0].start_time_ms, (kToday - 10) * kMsPerDay + 7);
        EXPECT_EQ(after.rows[1].start_time_ms, kToday * kMsPerDay + 7);
        EXPECT_DOUBLE_EQ(after.rows[1].score, 103.0);
    }

    TEST_F(CompletionHistoryUseCaseTest, ANewRunMovesALatestRunAnchoredRecentDaysInterval) {
        playOnDays({kToday - 400, kToday - 395, kToday - 392});
        setRange(RecentDays{5}, true);
        ASSERT_EQ(runNumbers(use_case.get_history()), (std::vector{2, 3}));

        playOnDays({kToday - 400, kToday - 395, kToday - 392, kToday - 380});
        session->notifyProfileChanged();
        const auto after = use_case.get_history();

        EXPECT_EQ(runNumbers(after), (std::vector{4})) << "the interval follows the new latest run, dropping older ones";
        EXPECT_EQ(after.full_count, 4);
    }

    TEST_F(CompletionHistoryUseCaseTest, ValidRangeWithNoRowsInsideIsEmptyButKeepsTheFullCount) {
        playOnDays({kToday - 40, kToday - 30});
        setRange(RecentDays{7});

        const auto history = use_case.get_history();

        EXPECT_TRUE(history.rows.empty());
        EXPECT_EQ(history.full_count, 2) << "an empty selection is not the same as no history";
    }

    TEST_F(CompletionHistoryUseCaseTest, NoHistoryHasZeroFullCountWhateverTheRange) {
        playOnDays({});
        setRange(RecentDays{7}, true);

        const auto history = use_case.get_history();

        EXPECT_TRUE(history.rows.empty());
        EXPECT_EQ(history.full_count, 0);
    }

    TEST_F(CompletionHistoryUseCaseTest, ASingleRowProjectionIsKept) {
        playOnDays({kToday - 40, kToday - 30, kToday});
        setRange(LatestRuns{1});

        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{3}));
    }

    TEST_F(CompletionHistoryUseCaseTest, SelectedRunKeepsItsOriginalIndexOutsideTheVisibleRows) {
        playOnDays({kToday - 40, kToday - 30, kToday});
        setCurrentScenario("scenario-1", (kToday - 40) * kMsPerDay + 7);
        setRange(LatestRuns{1});

        EXPECT_EQ(use_case.selected_run_index(), 1);
        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{3}));
    }

    TEST_F(CompletionHistoryUseCaseTest, ScoreAverageOfASingleRunIsItsOwnScore) {
        playOnDays({kToday});

        const auto history = use_case.get_history();

        ASSERT_EQ(history.rows.size(), 1);
        EXPECT_DOUBLE_EQ(history.rows[0].score_average, 100.0);
    }

    TEST_F(CompletionHistoryUseCaseTest, ScoreAverageUsesAllRunsWhenFewerThanTenExist) {
        playOnDays({kToday - 2, kToday - 1, kToday});

        const auto history = use_case.get_history();

        ASSERT_EQ(history.rows.size(), 3);
        EXPECT_DOUBLE_EQ(history.rows[0].score_average, 100.0);
        EXPECT_DOUBLE_EQ(history.rows[1].score_average, 100.5);
        EXPECT_DOUBLE_EQ(history.rows[2].score_average, 101.0);
    }

    TEST_F(CompletionHistoryUseCaseTest, ScoreAverageUsesAtMostTenRunsIncludingTheCurrentRun) {
        playOnDays({kToday - 10, kToday - 9, kToday - 8, kToday - 7, kToday - 6,
                    kToday - 5, kToday - 4, kToday - 3, kToday - 2, kToday - 1, kToday});

        const auto history = use_case.get_history();

        ASSERT_EQ(history.rows.size(), 11);
        EXPECT_DOUBLE_EQ(history.rows[0].score_average, 100.0);
        EXPECT_DOUBLE_EQ(history.rows[1].score_average, 100.5);
        EXPECT_DOUBLE_EQ(history.rows[9].score_average, 104.5);
        EXPECT_DOUBLE_EQ(history.rows[10].score_average, 105.5);
    }

    TEST_F(CompletionHistoryUseCaseTest, ScoreAverageUsesRunsBeforeTheVisibleRange) {
        playOnDays({kToday - 10, kToday - 9, kToday - 8, kToday - 7, kToday - 6,
                    kToday - 5, kToday - 4, kToday - 3, kToday - 2, kToday - 1, kToday});
        const double fullRangeValue = use_case.get_history().rows[10].score_average;
        setRange(LatestRuns{1});

        const auto history = use_case.get_history();

        ASSERT_EQ(history.rows.size(), 1);
        EXPECT_EQ(history.rows[0].run_index, 11);
        EXPECT_DOUBLE_EQ(history.rows[0].score_average, 105.5);
        EXPECT_DOUBLE_EQ(history.rows[0].score_average, fullRangeValue);
    }

    TEST_F(CompletionHistoryUseCaseTest, ScoreAverageUsesRunsBeforeAFilteredDateRange) {
        playOnDays({kToday - 40, kToday - 30, kToday - 20, kToday - 10, kToday});
        setRange(RecentDays{7});

        const auto history = use_case.get_history();

        ASSERT_EQ(history.rows.size(), 1);
        EXPECT_EQ(history.rows[0].run_index, 5);
        EXPECT_DOUBLE_EQ(history.rows[0].score_average, 102.0);
    }

    TEST_F(CompletionHistoryUseCaseTest, ScoreAverageIncludesZeroScores) {
        setCurrentScenario();
        profile->completion_history_by_scenario[{.name = "Scenario One", .hash = "scenario-1"}] = {
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 100},
             .totals = {.score = 0.0F}},
            {.run_id = {.scenario_id = {.name = "Scenario One", .hash = "scenario-1"}, .start_time = 200},
             .totals = {.score = 20.0F}},
        };

        const auto history = use_case.get_history();

        ASSERT_EQ(history.rows.size(), 2);
        EXPECT_DOUBLE_EQ(history.rows[0].score_average, 0.0);
        EXPECT_DOUBLE_EQ(history.rows[1].score_average, 10.0);
    }

    TEST_F(CompletionHistoryUseCaseTest, ScoreAverageOfAnEmptyHistoryHasNoRows) {
        playOnDays({});

        EXPECT_TRUE(use_case.get_history().rows.empty());
    }

    TEST_F(CompletionHistoryUseCaseTest, ScoreAverageDoesNotLeakBetweenScenarios) {
        playOnDays({kToday - 1, kToday}, "scenario-a");
        playOnDays({kToday}, "scenario-b");

        const auto second = use_case.get_history();
        ASSERT_EQ(second.rows.size(), 1);
        EXPECT_DOUBLE_EQ(second.rows[0].score_average, 100.0);

        setCurrentScenario("scenario-a", kToday * kMsPerDay + 7);
        const auto first = use_case.get_history();
        ASSERT_EQ(first.rows.size(), 2);
        EXPECT_DOUBLE_EQ(first.rows[1].score_average, 100.5);
    }

    TEST_F(CompletionHistoryUseCaseTest, SelectRunAddressesTheOriginalRowWhateverTheRange) {
        playOnDays({kToday - 40, kToday - 30, kToday});
        setRange(LatestRuns{1});

        use_case.select_run(3);

        ASSERT_TRUE(session->selected_run.has_value());
        EXPECT_EQ(session->selected_run->start_time, kToday * kMsPerDay + 7);
    }

    TEST_F(CompletionHistoryUseCaseTest, ChangingTheRangeNeverChangesTheSelectedRun) {
        playOnDays({kToday - 40, kToday - 30, kToday});
        setCurrentScenario("scenario-1", (kToday - 30) * kMsPerDay + 7);

        setRange(LatestRuns{1});
        commit();
        setRange(AllHistory{});
        commit();

        EXPECT_FALSE(session->selected_run.has_value());
        EXPECT_EQ(use_case.selected_run_index(), 2);
    }

    TEST_F(CompletionHistoryUseCaseTest, OneActiveRangeModeReplacesTheOther) {
        playOnDays({kToday - 40, kToday - 30, kToday - 20, kToday - 10, kToday});
        setRange(LatestRuns{1});
        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{5}));

        setRange(RecentDays{15});
        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{4, 5}))
            << "the stored range is a single discriminated value, so no earlier latest-N survives";
    }

    TEST_F(CompletionHistoryUseCaseTest, RangeChangeNotifiesOnlyForTheScenarioHistoryGraph) {
        int notifications = 0;
        use_case.onRangeChanged([&notifications] { ++notifications; });

        commit(GraphKey::DailyPlaytime);
        commit(GraphKey::BenchmarkRank);
        EXPECT_EQ(notifications, 0);

        commit(GraphKey::ScenarioHistory);
        EXPECT_EQ(notifications, 1);
    }

    TEST_F(CompletionHistoryUseCaseTest, DayChangeRefreshesOnlyTodayAnchoredPresets) {
        playOnDays({kToday - 6, kToday - 1});
        int notifications = 0;
        use_case.onRangeChanged([&notifications] { ++notifications; });

        setRange(RecentDays{7});
        days->advance();
        EXPECT_EQ(notifications, 1);
        EXPECT_EQ(runNumbers(use_case.get_history()), (std::vector{2})) << "the older run left the window";

        setRange(RecentDays{7}, true);
        days->advance();
        setRange(LatestRuns{1});
        days->advance();
        setRange(AllHistory{});
        days->advance();
        EXPECT_EQ(notifications, 1) << "anchored presets, latest-N and all history ignore the date";
    }

    TEST_F(CompletionHistoryUseCaseTest, StyleOnlyChangesNeverRequeryTheProfileOrRefreshTheRows) {
        playOnDays({kToday - 6, kToday - 1});
        std::ignore = use_case.get_history();
        int range = 0;
        int style = 0;
        use_case.onRangeChanged([&] { ++range; });
        use_case.onPresentationChanged([&] { ++style; });
        const auto queries = profile->completion_history_calls;

        preferences().appearance = {ConnectionMode::Straight, false};
        preferences().seriesStyles["hits"] = {RgbaColor{3, 3, 3, 255}, 7.0};
        preferences().axisPolicies["accuracy"] = StartAtZero{};
        configuration->publishDiff();

        EXPECT_EQ(range, 0);
        EXPECT_EQ(style, 1);
        EXPECT_EQ(profile->completion_history_calls, queries);
        const auto presentation = use_case.presentation();
        EXPECT_EQ(presentation.appearance, (LineAppearance{ConnectionMode::Straight, false}));
        EXPECT_EQ(presentation.seriesStyle("hits"), (SeriesStyle{RgbaColor{3, 3, 3, 255}, 7.0}));
    }

    TEST_F(CompletionHistoryUseCaseTest, ARangeChangeRefreshesTheRowsAndLeavesTheStyleAlone) {
        int range = 0;
        int style = 0;
        use_case.onRangeChanged([&] { ++range; });
        use_case.onPresentationChanged([&] { ++style; });

        setRange(LatestRuns{3});
        configuration->publishDiff();

        EXPECT_EQ(range, 1);
        EXPECT_EQ(style, 0);
    }

    TEST_F(CompletionHistoryUseCaseTest, TheStyleFollowsTheGraphAcrossScenarioChanges) {
        preferences().appearance.connection = ConnectionMode::PointsOnly;
        configuration->publishDiff();
        playOnDays({kToday}, "scenario-a");
        const auto first = use_case.presentation();
        playOnDays({kToday}, "scenario-b");

        EXPECT_EQ(use_case.presentation(), first);
        EXPECT_EQ(use_case.presentation().appearance.connection, ConnectionMode::PointsOnly);
    }
}
