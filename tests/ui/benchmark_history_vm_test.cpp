// BenchmarkHistoryViewModel is a GraphViewModelBase adapter. BenchmarkTrackingViewModel owns two
// of them - personal-best average rank and averaged playtime - and by default feeds both the same
// UTC-day X range while keeping metric-specific series names, transforms, and Y axes. These tests
// reach the child models through the tracking VM's rankHistory()/playtimeHistory() accessors.

#include <gtest/gtest.h>

#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTimeZone>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>

#include "components/graph_canvas.h"
#include "presentation/benchmark_format.h"
#include "presentation/benchmark_tracking_vm.h"
#include "presentation/playtime_graph_vm.h"
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
using ksv::tests_ui::expectedMidnightMs;
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
    EXPECT_DOUBLE_EQ(points[0].x(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 4))));
    EXPECT_DOUBLE_EQ(points[1].x(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 6))));
}

TEST(BenchmarkHistoryViewModel, RankLineIsHeldFlatToTheRightEdgeOfTheAxisWithoutAnObservationThere) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 6), 2.0}};
    // Playtime runs later than the last rank change, and the axis rounds up to a tick.
    snapshot.projection->rollingPlaytime = {{ymd(2024, 3, 5), 600.0}, {ymd(2024, 3, 20), 1200.0}};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.rankHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    const auto &rank = *series.front();
    ASSERT_EQ(rank.points.size(), 2);
    EXPECT_DOUBLE_EQ(rank.points[1].x(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 6))));
    EXPECT_FALSE(rank.leadingCarry);
    ASSERT_TRUE(rank.trailingCarry);
    EXPECT_DOUBLE_EQ(rank.trailingCarry->x(), vm.rankHistory()->xAxis().max());
    EXPECT_GT(rank.trailingCarry->x(), rank.points[1].x());
    EXPECT_DOUBLE_EQ(rank.trailingCarry->y(), 2.0);
}

TEST(BenchmarkHistoryViewModel, AnAnchoredWholeRankHistoryEndsAtItsLastPointNotTheSharedExtent) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 6), 2.0}};
    snapshot.projection->rollingPlaytime = {{ymd(2024, 3, 5), 600.0}, {ymd(2024, 3, 20), 1200.0}};
    snapshot.rank.anchoredEnd = ymd(2024, 3, 6);
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto &rank = *vm.rankHistory()->series({kValueColumn}).front();
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 4))));
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 6))));
    EXPECT_FALSE(rank.trailingCarry);
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 20))));
}

TEST(BenchmarkHistoryViewModel, BothHistoriesShareTheUnionOfTheirXRanges) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 6), 2.0}};
    snapshot.projection->rollingPlaytime = {{ymd(2024, 3, 5), 600.0}, {ymd(2024, 3, 7), 1200.0}};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto lo = static_cast<double>(expectedMidnightMs(ymd(2024, 3, 4)));
    const auto hi = static_cast<double>(expectedMidnightMs(ymd(2024, 3, 7)));
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
                     static_cast<double>(expectedMidnightMs(ymd(2024, 3, 4))));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(),
                     static_cast<double>(expectedMidnightMs(ymd(2024, 3, 5))));
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
        return rank.formattedValueAtX(static_cast<double>(expectedMidnightMs(ymd(2024, 3, day))));
    };
    EXPECT_EQ(at(4), "Unranked · 25% to Bronze");
    EXPECT_EQ(at(5), "Bronze · 60% to Silver");
    EXPECT_EQ(at(6), "Silver · 0% to Gold");
    EXPECT_EQ(at(7), "Gold · top tier reached");
    // The axis ticks keep their own labels: whole numbers are tier names.
    EXPECT_EQ(rank.yAxis->formatTick(2.0), "Silver");
}

TEST(BenchmarkHistoryViewModel, PlaytimeSeriesHasZeroBaselineSecondsToMinutesAndTheDefaultWindowLabel) {
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
    EXPECT_EQ(series.front()->name(), PlaytimeGraphViewModel::titleFor(kDefaultAveragingWindowDays));
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

TEST(BenchmarkHistoryViewModel, PlaytimeIsTitledExactlyLikeTheDailyPlaytimeGraph) {
    for (const int window: {1, 3, 7, 30}) {
        auto snapshot = makeTrackableSnapshot();
        snapshot.playtime.windowDays = window;
        auto fake = fakeWith(snapshot);
        BenchmarkTrackingViewModel vm{fake};

        const QString name = PlaytimeGraphViewModel::titleFor(window);
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
    EXPECT_EQ(vm.playtimeHistory()->metricName(), "Daily Playtime (10-day rolling average)");
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->points.front().y(), 1000.0);
}

TEST(BenchmarkHistoryViewModel, ADatePlaytimeRangeSpansItsIntervalWhileTheRankAxisKeepsTheDefaultExtent) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 6), 2.0}};
    snapshot.projection->rollingPlaytime = {{ymd(2024, 3, 4), 600.0}, {ymd(2024, 3, 12), 1200.0}};
    snapshot.playtime.fullExtent = DayInterval{ymd(2024, 3, 4), ymd(2024, 3, 12)};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};
    const auto rankMin = vm.rankHistory()->xAxis().min();
    const auto rankMax = vm.rankHistory()->xAxis().max();

    fake->snap.playtime.interval = DayInterval{ymd(2024, 3, 10), ymd(2024, 3, 12)};
    fake->snap.projection->rollingPlaytime = {{ymd(2024, 3, 12), 1200.0}};
    fake->publishPlaytime();

    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 10))));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 12))));
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), rankMin);
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), rankMax);
    EXPECT_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->points.size(), 1);
}

TEST(BenchmarkHistoryViewModel, AnEmptyPlaytimeIntervalKeepsItsAxisAndDrawsNoLine) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.playtime.fullExtent = DayInterval{ymd(2024, 3, 4), ymd(2024, 3, 6)};
    snapshot.playtime.interval = DayInterval{ymd(2024, 4, 1), ymd(2024, 4, 7)};
    snapshot.projection->rollingPlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(), static_cast<double>(expectedMidnightMs(ymd(2024, 4, 1))));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(), static_cast<double>(expectedMidnightMs(ymd(2024, 4, 7))));
    EXPECT_TRUE(vm.playtimeHistory()->series({kValueColumn}).isEmpty());
}

// ---- rank range and carried geometry ------------------------------------------------------

namespace {
    double ms(const std::chrono::sys_days day) { return static_cast<double>(expectedMidnightMs(day)); }

    // Improvements on 4, 6 and 10 March 2024; playtime spans 2 to 20 March.
    BenchmarkWorkspaceSnapshot rankRangeSnapshot(const std::optional<DayInterval> interval) {
        auto snapshot = makeTrackableSnapshot();
        snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 6), 2.0},
                                                   {ymd(2024, 3, 10), 2.5}};
        snapshot.projection->rollingPlaytime = {{ymd(2024, 3, 2), 600.0}, {ymd(2024, 3, 20), 1200.0}};
        snapshot.playtime.fullExtent = DayInterval{ymd(2024, 3, 2), ymd(2024, 3, 20)};
        snapshot.rank.interval = interval;
        return snapshot;
    }

    const SeriesModel *rankSeries(const BenchmarkTrackingViewModel &vm) {
        const auto series = vm.rankHistory()->series({kValueColumn});
        return series.isEmpty() ? nullptr : series.front();
    }

    [[nodiscard]] int countPixelsNear(const QImage &image, const QRect &area, const QColor &colour) {
        int count = 0;
        const QRect clipped = area.intersected(image.rect());
        for (int y = clipped.top(); y <= clipped.bottom(); ++y)
            for (int x = clipped.left(); x <= clipped.right(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                if (pixel.alpha() > 200 && std::abs(pixel.red() - colour.red()) <= 8 &&
                    std::abs(pixel.green() - colour.green()) <= 8 && std::abs(pixel.blue() - colour.blue()) <= 8)
                    ++count;
            }
        return count;
    }
}

TEST(BenchmarkHistoryViewModel, ARankRangeCarriesTheEstablishedRankInAndTheLastImprovementOut) {
    auto fake = fakeWith(rankRangeSnapshot(DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 12)}));
    BenchmarkTrackingViewModel vm{fake};

    const auto *rank = rankSeries(vm);
    ASSERT_NE(rank, nullptr);
    ASSERT_EQ(rank->points.size(), 2);
    EXPECT_EQ(rank->points[0], QPointF(ms(ymd(2024, 3, 6)), 2.0));
    EXPECT_EQ(rank->points[1], QPointF(ms(ymd(2024, 3, 10)), 2.5));
    ASSERT_TRUE(rank->leadingCarry);
    EXPECT_EQ(*rank->leadingCarry, QPointF(ms(ymd(2024, 3, 5)), 1.0));
    ASSERT_TRUE(rank->trailingCarry);
    EXPECT_EQ(*rank->trailingCarry, QPointF(ms(ymd(2024, 3, 12)), 2.5));

    auto *history = vm.rankHistory();
    EXPECT_TRUE(history->hasData());
    EXPECT_EQ(history->observationCount(), 2);
    EXPECT_TRUE(history->hasCarriedState());
    EXPECT_FALSE(history->noObservationsInRange());
    EXPECT_DOUBLE_EQ(history->xAxis().min(), ms(ymd(2024, 3, 5)));
    EXPECT_DOUBLE_EQ(history->xAxis().max(), ms(ymd(2024, 3, 12)));
}

TEST(BenchmarkHistoryViewModel, ARankRangeWithoutImprovementsDrawsOnlyNonSelectableCarriedGeometry) {
    auto fake = fakeWith(rankRangeSnapshot(DayInterval{ymd(2024, 3, 11), ymd(2024, 3, 13)}));
    BenchmarkTrackingViewModel vm{fake};

    const auto *rank = rankSeries(vm);
    ASSERT_NE(rank, nullptr) << "the carried rank is still drawn in line modes";
    EXPECT_TRUE(rank->points.isEmpty());
    ASSERT_TRUE(rank->leadingCarry);
    ASSERT_TRUE(rank->trailingCarry);
    EXPECT_EQ(*rank->leadingCarry, QPointF(ms(ymd(2024, 3, 11)), 2.5));
    EXPECT_EQ(*rank->trailingCarry, QPointF(ms(ymd(2024, 3, 13)), 2.5));

    auto *history = vm.rankHistory();
    EXPECT_TRUE(history->hasData());
    EXPECT_EQ(history->observationCount(), 0);
    EXPECT_TRUE(history->hasCarriedState());
    // What Points only has to explain: the range holds no improvement point.
    EXPECT_TRUE(history->noObservationsInRange());

    ksv::ui::GraphCanvas canvas;
    canvas.setWidth(400);
    canvas.setHeight(200);
    canvas.setGraphVm(history);
    canvas.setVisibleColumns({kValueColumn});
    canvas.setPointMarkers(true);
    canvas.setForegroundColor(QColor("#123456"));
    const QRectF plot = canvas.property("plotArea").toRectF();
    for (const qreal x: {plot.left() + 1, plot.center().x(), plot.right() - 1})
        EXPECT_FALSE(canvas.valuesAtX(x).value("valid").toBool()) << "tooltip snapped to an out-of-range point";
    ASSERT_TRUE(rank->yAxis);
    const qreal carryY = plot.bottom() - rank->yAxis->normalizedPosition(2.5) * plot.height();
    const qreal carryStartX = plot.left() + history->xAxis().normalizedPosition(rank->leadingCarry->x()) * plot.width();
    EXPECT_FALSE(canvas.nearestPoint(carryStartX, carryY).value("valid").toBool());
    EXPECT_FALSE(canvas.pointMarkersShown());

    QImage image(400, 200, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        canvas.paint(&painter);
    }
    EXPECT_GT(countPixelsNear(image, plot.toAlignedRect(), QColor("#123456")), 0) << "the carried line was not drawn";
}

TEST(BenchmarkHistoryViewModel, NoRankIsDrawnBeforeTheFirstImprovement) {
    auto fake = fakeWith(rankRangeSnapshot(DayInterval{ymd(2024, 3, 1), ymd(2024, 3, 3)}));
    BenchmarkTrackingViewModel vm{fake};

    auto *history = vm.rankHistory();
    EXPECT_FALSE(history->hasData());
    EXPECT_TRUE(history->series({kValueColumn}).isEmpty());
    EXPECT_FALSE(history->hasCarriedState());
    EXPECT_TRUE(history->noObservationsInRange());
    EXPECT_TRUE(history->emptyStateText().contains("range")) << history->emptyStateText().toStdString();

    fake->snap.rank.interval = DayInterval{ymd(2024, 3, 1), ymd(2024, 3, 5)};
    fake->publishRank();

    const auto *rank = rankSeries(vm);
    ASSERT_NE(rank, nullptr);
    ASSERT_EQ(rank->points.size(), 1);
    EXPECT_EQ(rank->points[0], QPointF(ms(ymd(2024, 3, 4)), 1.0));
    EXPECT_FALSE(rank->leadingCarry);
    ASSERT_TRUE(rank->trailingCarry);
    EXPECT_EQ(*rank->trailingCarry, QPointF(ms(ymd(2024, 3, 5)), 1.0));
}

TEST(BenchmarkHistoryViewModel, EveryRankPointIsAnActualImprovementWhateverTheRange) {
    const auto improvementDays = QList<double>{ms(ymd(2024, 3, 4)), ms(ymd(2024, 3, 6)), ms(ymd(2024, 3, 10))};
    for (const auto &interval: {std::optional<DayInterval>{}, std::optional(DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 8)}),
                                std::optional(DayInterval{ymd(2024, 3, 6), ymd(2024, 3, 10)}),
                                std::optional(DayInterval{ymd(2024, 3, 11), ymd(2024, 3, 30)})}) {
        auto fake = fakeWith(rankRangeSnapshot(interval));
        BenchmarkTrackingViewModel vm{fake};
        const auto *rank = rankSeries(vm);
        ASSERT_NE(rank, nullptr);
        for (const QPointF &point: rank->points) EXPECT_TRUE(improvementDays.contains(point.x())) << point.x();
    }
}

TEST(BenchmarkHistoryViewModel, TierReferencesKeepTheirFullHistoryMeaningInANarrowRange) {
    std::vector<Tier> tiers;
    for (int index = 1; index <= 6; ++index)
        tiers.push_back(coloredTier("t" + std::to_string(index), "Tier " + std::to_string(index),
                                    BenchmarkColor{200, 200, 200, 255}));
    auto snapshot = snapshotWithTiers(tiers);
    snapshot.projection->averageRankHistory = {{ymd(2024, 3, 4), 1.0}, {ymd(2024, 3, 8), 4.5}};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};
    const auto wholeMax = rankSeries(vm)->yAxis->max();
    const auto wholeLines = rankSeries(vm)->referenceLines.size();

    fake->snap.rank.interval = DayInterval{ymd(2024, 3, 4), ymd(2024, 3, 5)};
    fake->publishRank();

    const auto *narrowed = rankSeries(vm);
    ASSERT_NE(narrowed, nullptr);
    ASSERT_EQ(narrowed->points.size(), 1);
    EXPECT_DOUBLE_EQ(wholeMax, 5.0);
    EXPECT_DOUBLE_EQ(narrowed->yAxis->max(), wholeMax);
    EXPECT_EQ(narrowed->referenceLines.size(), wholeLines);
    EXPECT_EQ(narrowed->yAxis->formatTick(5.0), "Tier 5");
}

TEST(BenchmarkHistoryViewModel, RankAndPlaytimeRangesMoveOnlyTheirOwnAxes) {
    auto fake = fakeWith(rankRangeSnapshot(std::nullopt));
    BenchmarkTrackingViewModel vm{fake};
    const auto defaultMin = vm.playtimeHistory()->xAxis().min();
    const auto defaultMax = vm.playtimeHistory()->xAxis().max();
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), defaultMin);
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), defaultMax);

    fake->snap.rank.interval = DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 12)};
    fake->publishRank();
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), ms(ymd(2024, 3, 5)));
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), ms(ymd(2024, 3, 12)));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(), defaultMin);
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(), defaultMax);

    fake->snap.playtime.interval = DayInterval{ymd(2024, 3, 14), ymd(2024, 3, 16)};
    fake->snap.projection->rollingPlaytime = {};
    fake->publishPlaytime();
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(), ms(ymd(2024, 3, 14)));
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), ms(ymd(2024, 3, 5)));
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), ms(ymd(2024, 3, 12)));
}

TEST(BenchmarkHistoryViewModel, ARankRangeChangeLeavesTheSummaryAndPlaytimeAlone) {
    auto fake = fakeWith(rankRangeSnapshot(std::nullopt));
    BenchmarkTrackingViewModel vm{fake};
    const auto attained = vm.attainedRank();
    const auto average = vm.averageRank();
    const auto playtimePoints = vm.playtimeHistory()->series({kValueColumn}).front()->points;
    QSignalSpy changed(&vm, &BenchmarkTrackingViewModel::changed);

    fake->snap.rank.interval = DayInterval{ymd(2024, 3, 7), ymd(2024, 3, 9)};
    fake->publishRank();

    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(vm.attainedRank(), attained);
    EXPECT_EQ(vm.averageRank(), average);
    EXPECT_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->points, playtimePoints);
    EXPECT_TRUE(rankSeries(vm)->points.isEmpty());
}

TEST(BenchmarkHistoryViewModel, AnAnchoredRankRangeWithNoContributingRunShowsNothing) {
    auto snapshot = rankRangeSnapshot(std::nullopt);
    snapshot.rank.noRelevantHistory = true;
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_FALSE(vm.rankHistory()->hasData());
    EXPECT_TRUE(vm.rankHistory()->series({kValueColumn}).isEmpty());
}

// ---- content states: no improvement points, no relevant history ---------------------------

namespace {
    // Rank held at 2.5 through 11-13 March, with no improvement inside the range.
    BenchmarkWorkspaceSnapshot carriedOnlySnapshot() {
        return rankRangeSnapshot(DayInterval{ymd(2024, 3, 11), ymd(2024, 3, 13)});
    }
}

TEST(BenchmarkHistoryContent, PointsOnlyOverACarriedRankWithoutImprovementsHasNoImprovementPoints) {
    auto fake = fakeWith(carriedOnlySnapshot());
    fake->rankPresentation.appearance = {ConnectionMode::PointsOnly, false};
    BenchmarkTrackingViewModel vm{fake};
    const auto *history = vm.rankHistory();

    EXPECT_TRUE(history->hasCarriedState());
    EXPECT_EQ(history->observationCount(), 0);
    EXPECT_TRUE(history->noImprovementPoints());
    EXPECT_TRUE(history->property("noImprovementPoints").toBool());
}

TEST(BenchmarkHistoryContent, ALineModeDrawsTheCarriedRankSoThereIsNothingToExplain) {
    auto fake = fakeWith(carriedOnlySnapshot());
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_FALSE(vm.rankHistory()->noImprovementPoints());
    fake->rankPresentation.appearance = {ConnectionMode::Straight, false};
    fake->publishPresentation();
    EXPECT_FALSE(vm.rankHistory()->noImprovementPoints());
}

TEST(BenchmarkHistoryContent, SwitchingTheLineModeAnnouncesTheContentStateChange) {
    auto fake = fakeWith(carriedOnlySnapshot());
    BenchmarkTrackingViewModel vm{fake};
    const QSignalSpy spy(vm.rankHistory(), &BenchmarkHistoryViewModel::contentStateChanged);

    fake->rankPresentation.appearance = {ConnectionMode::PointsOnly, false};
    fake->publishPresentation();
    EXPECT_TRUE(vm.rankHistory()->noImprovementPoints());
    EXPECT_GE(spy.count(), 1);

    fake->rankPresentation.appearance = {ConnectionMode::Smooth, false};
    fake->publishPresentation();
    EXPECT_FALSE(vm.rankHistory()->noImprovementPoints());
}

TEST(BenchmarkHistoryContent, PointsOnlyWithImprovementsInTheRangeHasPointsToShow) {
    auto fake = fakeWith(rankRangeSnapshot(DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 12)}));
    fake->rankPresentation.appearance = {ConnectionMode::PointsOnly, false};
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.rankHistory()->observationCount(), 2);
    EXPECT_FALSE(vm.rankHistory()->noImprovementPoints());
}

TEST(BenchmarkHistoryContent, PointsOnlyBeforeAnyRankIsEstablishedIsAnEmptyRangeNotMissingImprovements) {
    auto fake = fakeWith(rankRangeSnapshot(DayInterval{ymd(2024, 3, 1), ymd(2024, 3, 3)}));
    fake->rankPresentation.appearance = {ConnectionMode::PointsOnly, false};
    BenchmarkTrackingViewModel vm{fake};
    const auto *history = vm.rankHistory();

    EXPECT_FALSE(history->hasCarriedState());
    EXPECT_FALSE(history->noImprovementPoints());
    EXPECT_TRUE(history->noObservationsInRange());
}

// EDGE-04: an anchor with no contributing run is the ordinary no-history state, even when stale
// history lingers in the projection.
TEST(BenchmarkHistoryContent, AnAnchoredRankRangeWithoutAContributingRunReadsAsNoHistory) {
    auto snapshot = rankRangeSnapshot(std::nullopt);
    snapshot.rank.noRelevantHistory = true;
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};
    const auto *history = vm.rankHistory();

    EXPECT_FALSE(history->historyAvailable());
    EXPECT_FALSE(history->noObservationsInRange());
    EXPECT_EQ(history->emptyStateText().toStdString(), "No history recorded yet.");
}

TEST(BenchmarkHistoryContent, APlaytimeRangeWithoutSamplesOverRecordedPlaytimeIsAnEmptyRange) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.playtime.fullExtent = DayInterval{ymd(2024, 3, 4), ymd(2024, 3, 6)};
    snapshot.playtime.interval = DayInterval{ymd(2024, 4, 1), ymd(2024, 4, 7)};
    snapshot.projection->rollingPlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};
    const auto *playtime = vm.playtimeHistory();

    EXPECT_TRUE(playtime->historyAvailable());
    EXPECT_TRUE(playtime->noObservationsInRange());
    EXPECT_FALSE(playtime->hasData());
    EXPECT_FALSE(playtime->noImprovementPoints());
}

// ── Numeric Y-axis policy (playtime metric only) ─────────────────────────────────────────────────

namespace {
    const ValueAxis &historyAxis(const BenchmarkHistoryViewModel &vm) {
        const auto series = vm.series({kValueColumn});
        EXPECT_FALSE(series.isEmpty());
        EXPECT_TRUE(series.front()->yAxis.has_value());
        return *series.front()->yAxis;
    }

    BenchmarkHistoryViewModel::Content secondsContent(const std::vector<double> &seconds) {
        BenchmarkHistoryViewModel::Content content;
        content.historyAvailable = true;
        double x = 0.0;
        for (const double value: seconds) content.observations.append(QPointF(x++, value));
        return content;
    }

    void expectSameAxis(const Axis &actual, const Axis &expected) {
        EXPECT_DOUBLE_EQ(actual.min(), expected.min());
        EXPECT_DOUBLE_EQ(actual.max(), expected.max());
        EXPECT_EQ(actual.ticks(), expected.ticks());
    }

    const std::vector<RankTier> kTiers{{"Bronze", QColor("brown")}, {"Silver", QColor("gray")}, {"Gold", QColor("gold")}};
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimeDefaultsToTheZeroBasedMinutesAxis) {
    ValueAxis x;
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime, x);
    vm.update(secondsContent({1800.0, 4200.0}), true, {});

    ValueAxis expected({.baseline = ValueAxis::Baseline::Zero}, ValueTransform::secondsToMinutes());
    expected.setRange(30.0, 70.0);
    expectSameAxis(historyAxis(vm), expected);
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimeFitsFixesAndRestoresItsAxisInMinutes) {
    ValueAxis x;
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime, x);
    vm.update(secondsContent({1800.0, 4200.0}), true, {});

    vm.setAxisPolicies({{"playtime", FitVisibleData{}}});
    ValueAxis fitted({}, ValueTransform::secondsToMinutes());
    fitted.setRange(30.0, 70.0);
    expectSameAxis(historyAxis(vm), fitted);

    vm.setAxisPolicies({{"playtime", FixedBounds{10.0, 45.0}}});
    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 10.0);
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 45.0);
    EXPECT_EQ(historyAxis(vm).formatTick(45.0), "45 min");

    vm.setAxisPolicies({});
    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 0.0);
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimePolicySurvivesFurtherUpdates) {
    ValueAxis x;
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime, x);
    vm.setAxisPolicies({{"playtime", FixedBounds{10.0, 45.0}}});

    vm.update(secondsContent({600.0}), true, {});
    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 10.0);
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 45.0);

    vm.update(secondsContent({600.0, 7200.0}), true, {});
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 45.0);
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimeFitOfASingleDayUsesTheFallbackSpan) {
    ValueAxis x;
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime, x);
    vm.setAxisPolicies({{"playtime", FitVisibleData{}}});

    vm.update(secondsContent({1800.0}), true, {});

    ValueAxis expected({}, ValueTransform::secondsToMinutes());
    expected.setRange(30.0, 30.0);
    expectSameAxis(historyAxis(vm), expected);
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimePolicyChangesRepaintOnlyWhenTheyChangeSomething) {
    ValueAxis x;
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime, x);
    vm.update(secondsContent({1800.0}), true, {});
    const QSignalSpy bounds(&vm, &GraphViewModelBase::boundsChanged);

    vm.setAxisPolicies({{"playtime", FitVisibleData{}}});
    vm.setAxisPolicies({{"playtime", FitVisibleData{}}});

    EXPECT_EQ(bounds.count(), 1);
}

TEST(BenchmarkHistoryAxisPolicy, TheRankGraphNeverTakesGenericNumericFitting) {
    ValueAxis x;
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank, x);
    BenchmarkHistoryViewModel::Content content;
    content.historyAvailable = true;
    content.observations = {QPointF(0.0, 1.0), QPointF(1.0, 2.0)};
    content.fullHistoryPeak = 2.0;
    vm.update(content, true, kTiers);
    const ValueAxis tierAxis = historyAxis(vm);
    const QSignalSpy bounds(&vm, &GraphViewModelBase::boundsChanged);

    vm.setAxisPolicies({{"playtime", FixedBounds{10.0, 45.0}}, {"rank", FitVisibleData{}}});
    expectSameAxis(historyAxis(vm), tierAxis);
    vm.update(content, true, kTiers);

    expectSameAxis(historyAxis(vm), tierAxis);
    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 0.0);
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 3.0);
    EXPECT_EQ(historyAxis(vm).formatTick(2.0), "Silver");
    EXPECT_EQ(bounds.count(), 1) << "only the update announced anything";
}


// ---- committed style ----------------------------------------------------------------------

TEST(BenchmarkHistoryStyle, WithNothingSavedBothGraphsLookAsTheyAlwaysDid) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto rankDefaults = defaultGraphPresentation(GraphKey::BenchmarkRank);
    const auto playtimeDefaults = defaultGraphPresentation(GraphKey::BenchmarkPlaytime);
    const auto rankStyle = rankDefaults.seriesStyle("rank");
    const auto playtimeStyle = playtimeDefaults.seriesStyle("playtime");

    EXPECT_EQ(vm.rankHistory()->property("connectionMode").toInt(), static_cast<int>(rankDefaults.appearance.connection));
    EXPECT_EQ(vm.rankHistory()->property("pointMarkers").toBool(), rankDefaults.appearance.lineModeMarkers);
    EXPECT_DOUBLE_EQ(vm.rankHistory()->series({kValueColumn}).front()->lineWidth, rankStyle.width);
    EXPECT_EQ(vm.playtimeHistory()->property("connectionMode").toInt(),
              static_cast<int>(playtimeDefaults.appearance.connection));
    EXPECT_EQ(vm.playtimeHistory()->property("pointMarkers").toBool(), playtimeDefaults.appearance.lineModeMarkers);
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->lineWidth, playtimeStyle.width);
    EXPECT_FALSE(vm.rankHistory()->series({kValueColumn}).front()->color().isValid()) << "neutral rank line";
    ASSERT_TRUE(playtimeStyle.color.has_value());
    EXPECT_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->color(),
              QColor(playtimeStyle.color->red, playtimeStyle.color->green, playtimeStyle.color->blue,
                     playtimeStyle.color->alpha));
}

TEST(BenchmarkHistoryStyle, TheCommittedStyleIsAppliedAtConstructionToEachGraph) {
    auto fake = fakeWith(makeTrackableSnapshot());
    fake->rankPresentation.appearance = {ConnectionMode::Straight, true};
    fake->rankPresentation.seriesStyles["rank"] = {RgbaColor{200, 0, 0, 255}, 6.0};
    fake->playtimePresentation.appearance = {ConnectionMode::PointsOnly, false};

    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.rankHistory()->property("connectionMode").toInt(), static_cast<int>(ConnectionMode::Straight));
    EXPECT_TRUE(vm.rankHistory()->property("pointMarkers").toBool());
    const auto *rank = vm.rankHistory()->series({kValueColumn}).front();
    EXPECT_EQ(rank->color(), QColor(200, 0, 0));
    EXPECT_DOUBLE_EQ(rank->lineWidth, 6.0);
    EXPECT_EQ(vm.playtimeHistory()->property("connectionMode").toInt(), static_cast<int>(ConnectionMode::PointsOnly));
    EXPECT_FALSE(vm.playtimeHistory()->property("pointMarkers").toBool());
}

TEST(BenchmarkHistoryStyle, AStyleChangeRestylesWithoutReadingTheWorkspaceAgain) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    const int reads = fake->snapshotCalls;
    const QSignalSpy rankStyle(vm.rankHistory(), &GraphViewModelBase::styleChanged);
    const QSignalSpy playtimeStyle(vm.playtimeHistory(), &GraphViewModelBase::styleChanged);
    const QSignalSpy changed(&vm, &BenchmarkTrackingViewModel::changed);

    fake->playtimePresentation.seriesStyles["playtime"] = {RgbaColor{9, 8, 7, 255}, 9.0};
    fake->publishPresentation();

    EXPECT_EQ(fake->snapshotCalls, reads);
    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(playtimeStyle.count(), 1);
    EXPECT_EQ(rankStyle.count(), 0) << "the rank graph style did not move";
    const auto *playtime = vm.playtimeHistory()->series({kValueColumn}).front();
    EXPECT_EQ(playtime->color(), QColor(9, 8, 7));
    EXPECT_DOUBLE_EQ(playtime->lineWidth, 9.0);
}

// STYLE-07: recolouring the rank line must not leave it unidentifiable.
TEST(BenchmarkHistoryStyle, ARecolouredRankSeriesKeepsItsIdentifyingNameAndTierReferences) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    const auto tierColours = [&] {
        QList<QColor> colours;
        for (const auto &line: vm.rankHistory()->series({kValueColumn}).front()->referenceLines)
            colours.append(line.color);
        return colours;
    };
    const auto tiersBefore = tierColours();

    fake->rankPresentation.seriesStyles["rank"] = {RgbaColor{12, 34, 56, 255}, std::nullopt};
    fake->publishPresentation();
    fake->publishRank();

    const auto *rank = vm.rankHistory()->series({kValueColumn}).front();
    EXPECT_EQ(rank->color(), QColor(12, 34, 56));
    EXPECT_EQ(rank->name(), "Personal-best average rank");
    EXPECT_EQ(vm.rankHistory()->metricName(), "Personal-best average rank");
    EXPECT_EQ(tierColours(), tiersBefore);
}

TEST(BenchmarkHistoryStyle, TheStyleSurvivesRebuildsAndBenchmarkSelectionChanges) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    fake->rankPresentation.seriesStyles["rank"] = {RgbaColor{1, 1, 1, 255}, 5.0};
    fake->playtimePresentation.appearance = {ConnectionMode::Straight, true};
    fake->publishPresentation();

    auto other = makeTrackableSnapshot();
    other.selectedId = BenchmarkId{"b2"};
    other.projection->averageRankHistory = {{ymd(2025, 1, 1), 2.0}, {ymd(2025, 1, 9), 3.0}};
    other.projection->rollingPlaytime = {{ymd(2025, 1, 1), 60.0}, {ymd(2025, 1, 9), 120.0}};
    fake->snap = other;
    fake->publish();
    fake->publishPlaytime();
    fake->publishRank();

    const auto *rank = vm.rankHistory()->series({kValueColumn}).front();
    EXPECT_EQ(rank->color(), QColor(1, 1, 1));
    EXPECT_DOUBLE_EQ(rank->lineWidth, 5.0);
    EXPECT_EQ(vm.playtimeHistory()->property("connectionMode").toInt(), static_cast<int>(ConnectionMode::Straight));
    EXPECT_TRUE(vm.playtimeHistory()->property("pointMarkers").toBool());
}

TEST(BenchmarkHistoryStyle, OnlyThePlaytimeGraphTakesAnAxisPolicyAndTheRankAxisStaysTierBased) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    const auto rankAxisMax = vm.rankHistory()->series({kValueColumn}).front()->yAxis->max();

    fake->playtimePresentation.axisPolicies["playtime"] = FixedBounds{5.0, 45.0};
    fake->rankPresentation.axisPolicies["playtime"] = FixedBounds{5.0, 45.0};
    fake->publishPresentation();

    const auto &playtimeAxis = *vm.playtimeHistory()->series({kValueColumn}).front()->yAxis;
    EXPECT_DOUBLE_EQ(playtimeAxis.min(), 5.0);
    EXPECT_DOUBLE_EQ(playtimeAxis.max(), 45.0);
    EXPECT_DOUBLE_EQ(vm.rankHistory()->series({kValueColumn}).front()->yAxis->max(), rankAxisMax);
}
