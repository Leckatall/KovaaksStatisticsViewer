// BenchmarkHistoryViewModel is a GraphViewModelBase adapter. BenchmarkTrackingViewModel owns two
// of them - personal-best average rank and averaged playtime - and by default feeds both the same
// UTC-day X range while keeping metric-specific series names, transforms, and Y axes. These tests
// reach the child models through the tracking VM's rankHistory()/playtimeHistory() accessors.

#include <gtest/gtest.h>

#include <QDateTime>
#include <QSignalSpy>
#include <QTimeZone>
#include <memory>

#include "components/graph_canvas.h"
#include "presentation/benchmark_format.h"
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

TEST(BenchmarkHistoryViewModel, EachHistoryOwnsItsXAxisSoOnePlaytimeRangeNeverMovesTheOther) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_NE(&vm.rankHistory()->xAxis(), &vm.playtimeHistory()->xAxis());
}

TEST(BenchmarkHistoryViewModel, CalendarDaysConvertToUtcMidnightEpochMs) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    const auto points = series.front()->points;
    ASSERT_GE(points.size(), 2);
    EXPECT_DOUBLE_EQ(points[0].x(), static_cast<double>(utcMidnightMs(ymd(2024, 3, 4))));
    EXPECT_DOUBLE_EQ(points[1].x(), static_cast<double>(utcMidnightMs(ymd(2024, 3, 6))));
}

TEST(BenchmarkHistoryViewModel, RankLineIsHeldFlatToTheRightEdgeOfTheAxis) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 6), 2.0}};
    // Playtime runs later than the last rank change, and the axis rounds up to a tick.
    snapshot.projection->rollingPlaytime = {{ymd(2024, 3, 5), 600.0}, {ymd(2024, 3, 20), 1200.0}};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    const auto points = series.front()->points;
    ASSERT_EQ(points.size(), 3);
    EXPECT_DOUBLE_EQ(points[1].x(), static_cast<double>(utcMidnightMs(ymd(2024, 3, 6))));
    EXPECT_DOUBLE_EQ(points[2].x(), vm.rankHistory()->xAxis().max());
    EXPECT_GT(points[2].x(), points[1].x());
    EXPECT_DOUBLE_EQ(points[2].y(), 2.0);
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

TEST(BenchmarkHistoryViewModel, RankTooltipValueReadsAsTheTierAndProgressTowardTheNext) {
    auto snapshot = makeTrackableSnapshot();
    // Held flat to the right edge, so every x from the first point on samples the last rank.
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 0.25}, {ymd(2024, 3, 5), 1.6},
                                               {ymd(2024, 3, 6), 2.0}, {ymd(2024, 3, 7), 3.0}};
    snapshot.projection->rollingPlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    const auto &rank = *series.front();
    const auto at = [&](const int day) {
        return rank.formattedValueAtX(static_cast<double>(utcMidnightMs(ymd(2024, 3, day))));
    };
    EXPECT_EQ(at(4), "Unranked · 25% to Bronze");
    EXPECT_EQ(at(5), "Bronze · 60% to Silver");
    EXPECT_EQ(at(6), "Silver · 0% to Gold");
    EXPECT_EQ(at(7), "Gold · top tier reached");
    // The axis ticks keep their own labels: whole numbers are tier names.
    EXPECT_EQ(rank.yAxis->formatTick(2.0), "Silver");
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

namespace {
    Tier coloredTier(const std::string &id, const std::string &name, const BenchmarkColor color) {
        return {TierId{id}, name, color};
    }

    BenchmarkWorkspaceSnapshot snapshotWithTiers(std::vector<Tier> tiers) {
        auto snapshot = makeTrackableSnapshot();
        snapshot.selectedLoaded->benchmark.tiers = tiers;
        snapshot.projection->tiers = std::move(tiers);
        return snapshot;
    }
}

TEST(BenchmarkHistoryViewModel, RankSeriesDrawsOneLineInEachTiersColourAtItsLevel) {
    const BenchmarkColor iron{130, 130, 140, 255};
    const BenchmarkColor gold{255, 200, 40, 255};
    const BenchmarkColor dark{20, 20, 60, 255};
    auto fake = fakeWith(snapshotWithTiers({coloredTier("t1", "Iron", iron), coloredTier("t2", "Gold", gold),
                                            coloredTier("t3", "Dark", dark)}));
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    const auto &lines = series.front()->referenceLines;
    ASSERT_EQ(lines.size(), 3);
    EXPECT_DOUBLE_EQ(lines[0].value, 1.0);
    EXPECT_DOUBLE_EQ(lines[1].value, 2.0);
    EXPECT_DOUBLE_EQ(lines[2].value, 3.0);
    // The same readable colours as the tier names elsewhere on the page.
    EXPECT_EQ(lines[0].color, readableTextColor(iron));
    EXPECT_EQ(lines[1].color, readableTextColor(gold));
    EXPECT_EQ(lines[2].color, readableTextColor(dark));
    EXPECT_NE(lines[2].color, QColor(20, 20, 60));
}

TEST(BenchmarkHistoryViewModel, RankSeriesIsNeutralSoItIsNotMistakenForATierLine) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    EXPECT_FALSE(series.front()->color().isValid());
}

TEST(BenchmarkHistoryViewModel, PlaytimeSeriesKeepsItsColourAndHasNoTierLines) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.playtimeHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    EXPECT_TRUE(series.front()->color().isValid());
    EXPECT_TRUE(series.front()->referenceLines.isEmpty());
}

TEST(BenchmarkHistoryViewModel, RankAxisHasATickAtEveryTierEvenWithManyTiers) {
    for (const int tierCount: {3, 5, 7, 12, 13, 25}) {
        std::vector<Tier> tiers;
        for (int index = 1; index <= tierCount; ++index)
            tiers.push_back(coloredTier("t" + std::to_string(index), "Tier " + std::to_string(index),
                                        BenchmarkColor{200, 200, 200, 255}));
        auto snapshot = snapshotWithTiers(std::move(tiers));
        snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), static_cast<double>(tierCount)}};
        auto fake = fakeWith(snapshot);
        BenchmarkTrackingViewModel vm{fake};

        const auto series = vm.rankHistory()->series({kValueColumn});
        ASSERT_EQ(series.size(), 1) << tierCount;
        ASSERT_TRUE(series.front()->yAxis.has_value()) << tierCount;
        QList<qreal> expected;
        for (int k = 0; k <= tierCount; ++k) expected.append(k);
        EXPECT_EQ(series.front()->yAxis->ticks(), expected) << tierCount;
        EXPECT_EQ(series.front()->referenceLines.size(), tierCount);
    }
}

TEST(BenchmarkHistoryViewModel, RankAxisAndTierLinesStopOneTierAboveThePeakRank) {
    struct Case {
        double peak;
        int shownTiers;
    };
    for (const auto [peak, shownTiers]: {Case{0.25, 1}, Case{1.6, 2}, Case{2.0, 3}, Case{4.5, 5}, Case{5.0, 5}}) {
        std::vector<Tier> tiers;
        for (int index = 1; index <= 5; ++index)
            tiers.push_back(coloredTier("t" + std::to_string(index), "Tier " + std::to_string(index),
                                        BenchmarkColor{200, 200, 200, 255}));
        auto snapshot = snapshotWithTiers(std::move(tiers));
        // The peak is not the last point, so the cap follows the best rank rather than the current one.
        snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), peak}, {ymd(2024, 3, 5), 0.1}};
        auto fake = fakeWith(snapshot);
        BenchmarkTrackingViewModel vm{fake};

        const auto series = vm.rankHistory()->series({kValueColumn});
        ASSERT_EQ(series.size(), 1) << peak;
        ASSERT_TRUE(series.front()->yAxis.has_value()) << peak;
        EXPECT_DOUBLE_EQ(series.front()->yAxis->max(), shownTiers) << peak;
        EXPECT_EQ(series.front()->referenceLines.size(), shownTiers) << peak;
        EXPECT_EQ(series.front()->yAxis->formatTick(shownTiers), "Tier " + QString::number(shownTiers)) << peak;
    }
}

TEST(BenchmarkHistoryViewModel, CanvasDrawsTheNeutralRankLineInItsForegroundColour) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    ksv::ui::GraphCanvas canvas;
    canvas.setWidth(400);
    canvas.setHeight(200);
    canvas.setGraphVm(vm.rankHistory());
    canvas.setVisibleColumns({kValueColumn});
    canvas.setForegroundColor(QColor("#123456"));

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    EXPECT_EQ(canvas.colorFor(*series.front()), QColor("#123456"));

    const QRectF plot = canvas.property("plotArea").toRectF();
    const QVariantMap values = canvas.valuesAtX(plot.left() + 1);
    ASSERT_TRUE(values.value("valid").toBool());
    const QVariantList entries = values.value("series").toList();
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries.front().toMap().value("color").value<QColor>(), QColor("#123456"));
}

TEST(BenchmarkHistoryViewModel, CanvasKeepsAColouredSeriesOwnColour) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    ksv::ui::GraphCanvas canvas;
    canvas.setForegroundColor(QColor("#123456"));
    const auto series = vm.playtimeHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    EXPECT_EQ(canvas.colorFor(*series.front()), series.front()->color());
}

TEST(BenchmarkHistoryViewModel, PlaytimeNamesTheEffectiveWindowAndNeverCallsAnotherWindowThreeDay) {
    const std::pair<int, QString> expected[] = {
        {1, "Daily playtime"}, {3, "Three-day average playtime"}, {7, "7-day average playtime"},
        {30, "30-day average playtime"}};
    for (const auto &[window, name]: expected) {
        auto snapshot = makeTrackableSnapshot();
        snapshot.playtime.windowDays = window;
        auto fake = fakeWith(snapshot);
        BenchmarkTrackingViewModel vm{fake};

        EXPECT_EQ(vm.playtimeHistory()->metricName(), name);
        EXPECT_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->name(), name);
        EXPECT_EQ(vm.rankHistory()->metricName(), "Personal-best average rank");
    }
}

TEST(BenchmarkHistoryViewModel, AWindowChangeRelabelsPlaytimeWithoutRebuildingTheWholeWorkspace) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    QSignalSpy changed(&vm, &BenchmarkTrackingViewModel::changed);
    QSignalSpy renamed(vm.playtimeHistory(), &BenchmarkHistoryViewModel::metricNameChanged);

    fake->snap.playtime.windowDays = 10;
    fake->snap.projection->rollingPlaytime = {{ymd(2024, 3, 4), 1000.0}, {ymd(2024, 3, 6), 500.0}};
    fake->publishPlaytime();

    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(renamed.count(), 1);
    EXPECT_EQ(vm.playtimeHistory()->metricName(), "10-day average playtime");
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->points.front().y(), 1000.0);
}

TEST(BenchmarkHistoryViewModel, ADatePlaytimeRangeSpansItsIntervalWhileTheRankAxisKeepsTheDefaultExtent) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 6), 2.0}};
    snapshot.projection->rollingPlaytime = {{ymd(2024, 3, 4), 600.0}, {ymd(2024, 3, 12), 1200.0}};
    snapshot.playtime.fullExtent = PlaytimeDayExtent{ymd(2024, 3, 4), ymd(2024, 3, 12)};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};
    const auto rankMin = vm.rankHistory()->xAxis().min();
    const auto rankMax = vm.rankHistory()->xAxis().max();

    fake->snap.playtime.interval = DayInterval{ymd(2024, 3, 10), ymd(2024, 3, 12)};
    fake->snap.projection->rollingPlaytime = {{ymd(2024, 3, 12), 1200.0}};
    fake->publishPlaytime();

    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(), static_cast<double>(utcMidnightMs(ymd(2024, 3, 10))));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(), static_cast<double>(utcMidnightMs(ymd(2024, 3, 12))));
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), rankMin);
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), rankMax);
    EXPECT_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->points.size(), 1);
}

TEST(BenchmarkHistoryViewModel, AnEmptyPlaytimeIntervalKeepsItsAxisAndDrawsNoLine) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.playtime.fullExtent = PlaytimeDayExtent{ymd(2024, 3, 4), ymd(2024, 3, 6)};
    snapshot.playtime.interval = DayInterval{ymd(2024, 4, 1), ymd(2024, 4, 7)};
    snapshot.projection->rollingPlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(), static_cast<double>(utcMidnightMs(ymd(2024, 4, 1))));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(), static_cast<double>(utcMidnightMs(ymd(2024, 4, 7))));
    EXPECT_TRUE(vm.playtimeHistory()->series({kValueColumn}).isEmpty());
}
