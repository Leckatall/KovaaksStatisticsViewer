// BenchmarkHistoryViewModel is a GraphViewModelBase adapter. BenchmarkTrackingViewModel owns two
// of them - personal-best average rank and three-day average playtime - and feeds both the same
// UTC-day X range while keeping metric-specific series names, transforms, and Y axes. These tests
// reach the child models through the tracking VM's rankHistory()/playtimeHistory() accessors.

#include <gtest/gtest.h>

#include <QDateTime>
#include <QTimeZone>
#include <memory>

#include "presentation/benchmark_tracking_vm.h"
#include "presentation/benchmark_history_vm.h"
#include "presentation/graph_vm_base.h"
#include "presentation/series_model.h"
#include "presentation/value_axis.h"

#include "fake_benchmark_tracking_use_case.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;
using ksv::tests_ui::FakeBenchmarkTrackingUseCase;
using ksv::tests_ui::makeTrackableSnapshot;
using ksv::tests_ui::utcMidnightMs;
using ksv::tests_ui::ymd;

namespace {
    // Date is the axis-only column; Value is the single drawn series - mirrors
    // PlaytimeGraphViewModel::{Date, Playtime}. Raw ids, per graph_vm_test.cpp convention.
    constexpr int kValueColumn = 1;

    std::shared_ptr<FakeBenchmarkTrackingUseCase> fakeWith(BenchmarkWorkspaceSnapshot snapshot) {
        auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
        fake->snap = std::move(snapshot);
        return fake;
    }
}

TEST(BenchmarkHistoryViewModel, RankAndPlaytimeAreSeparateChildObjects) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    ASSERT_NE(vm.rankHistory(), nullptr);
    ASSERT_NE(vm.playtimeHistory(), nullptr);
    EXPECT_NE(static_cast<QObject *>(vm.rankHistory()), static_cast<QObject *>(vm.playtimeHistory()));
}

TEST(BenchmarkHistoryViewModel, BothHistoriesShareTheSameXAxisAddress) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(&vm.rankHistory()->xAxis(), &vm.playtimeHistory()->xAxis());
}

TEST(BenchmarkHistoryViewModel, CalendarDaysConvertToUtcMidnightEpochMs) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    const auto points = series.front()->points;
    ASSERT_EQ(points.size(), 2);
    EXPECT_DOUBLE_EQ(points[0].x(), static_cast<double>(utcMidnightMs(ymd(2024, 3, 4))));
    EXPECT_DOUBLE_EQ(points[1].x(), static_cast<double>(utcMidnightMs(ymd(2024, 3, 6))));
}

TEST(BenchmarkHistoryViewModel, BothHistoriesShareTheUnionOfTheirXRanges) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 6), 2.0}};
    snapshot.projection->rollingPlaytime = {{ymd(2024, 3, 5), 600.0}, {ymd(2024, 3, 7), 1200.0}};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto lo = static_cast<double>(utcMidnightMs(ymd(2024, 3, 4)));
    const auto hi = static_cast<double>(utcMidnightMs(ymd(2024, 3, 7)));
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), lo);
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), hi);
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(), lo);
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(), hi);
}

TEST(BenchmarkHistoryViewModel, SingleHistoryRangeIsUsedForBoth) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 5), 2.0}};
    snapshot.projection->rollingPlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(),
                     static_cast<double>(utcMidnightMs(ymd(2024, 3, 4))));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(),
                     static_cast<double>(utcMidnightMs(ymd(2024, 3, 5))));
}

TEST(BenchmarkHistoryViewModel, NoHistoryYieldsDegenerateCurrentDayRangeAndNoLine) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory.clear();
    snapshot.projection->rollingPlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto nowMs = static_cast<double>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    for (GraphViewModelBase *history : {vm.rankHistory(), vm.playtimeHistory()}) {
        EXPECT_LT(history->xAxis().min(), nowMs);
        EXPECT_GT(history->xAxis().max(), nowMs);
        EXPECT_TRUE(history->series({kValueColumn}).isEmpty());
    }
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), vm.playtimeHistory()->xAxis().min());
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), vm.playtimeHistory()->xAxis().max());

    auto *rank = qobject_cast<BenchmarkHistoryViewModel *>(vm.rankHistory());
    ASSERT_NE(rank, nullptr);
    EXPECT_FALSE(rank->hasData());
    EXPECT_FALSE(rank->emptyStateText().isEmpty());
}

TEST(BenchmarkHistoryViewModel, RankSeriesSpansUnrankedZeroThroughTierCountWithTierLabels) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    ASSERT_TRUE(series.front()->yAxis.has_value());
    const ValueAxis &yAxis = *series.front()->yAxis;

    EXPECT_DOUBLE_EQ(yAxis.min(), 0.0);
    EXPECT_DOUBLE_EQ(yAxis.max(), 3.0);
    EXPECT_EQ(yAxis.formatTick(0.0), "Unranked");
    EXPECT_EQ(yAxis.formatTick(1.0), "Bronze");
    EXPECT_EQ(yAxis.formatTick(2.0), "Silver");
    EXPECT_EQ(yAxis.formatTick(3.0), "Gold");
    // Fractional tooltip values keep numeric formatting rather than snapping to a tier name.
    EXPECT_EQ(yAxis.formatTick(1.5), "1.5");
    EXPECT_TRUE(series.front()->name().contains("personal-best average rank", Qt::CaseInsensitive));
}

TEST(BenchmarkHistoryViewModel, PlaytimeSeriesHasZeroBaselineSecondsToMinutesAndThreeDayLabel) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.playtimeHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    const auto displayed = series.front()->displayPoints();
    ASSERT_EQ(displayed.size(), 2);
    EXPECT_DOUBLE_EQ(displayed[0].y(), 30.0);
    EXPECT_DOUBLE_EQ(displayed[1].y(), 15.0);
    ASSERT_TRUE(series.front()->yAxis.has_value());
    EXPECT_DOUBLE_EQ(series.front()->yAxis->min(), 0.0);
    EXPECT_TRUE(series.front()->name().contains("three-day average playtime", Qt::CaseInsensitive));
}

TEST(BenchmarkHistoryViewModel, RankAndPlaytimeKeepSeparateVerticalScales) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto rank = vm.rankHistory()->series({kValueColumn});
    const auto playtime = vm.playtimeHistory()->series({kValueColumn});
    ASSERT_EQ(rank.size(), 1);
    ASSERT_EQ(playtime.size(), 1);
    ASSERT_TRUE(rank.front()->yAxis.has_value());
    ASSERT_TRUE(playtime.front()->yAxis.has_value());
    EXPECT_NE(rank.front()->yAxis->max(), playtime.front()->yAxis->max());
}

TEST(BenchmarkHistoryViewModel, IncompleteDefinitionHasNoRankLineButKeepsPlaytime) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.selectedLoaded->completeness =
        CompletenessResult{Completeness::Incomplete,
                           {BenchmarkIssue{BenchmarkIssueCode::NoTiers, std::monostate{}}}};
    snapshot.projection->completeness = Completeness::Incomplete;
    snapshot.projection->averageRankHistory.clear();
    snapshot.projection->attainedRank.reset();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_TRUE(vm.rankHistory()->series({kValueColumn}).isEmpty());
    EXPECT_FALSE(vm.playtimeHistory()->series({kValueColumn}).isEmpty());
}
