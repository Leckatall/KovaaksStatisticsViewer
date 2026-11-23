// BenchmarkHistoryViewModel is a GraphViewModelBase adapter. BenchmarkTrackingViewModel owns three
// of them - personal-best average rank, recent average rank and averaged playtime - and by default
// feeds all of them the same UTC-day X range while keeping metric-specific series names, delegates,
// and Y axes. These tests reach the child models through the tracking VM's rankHistory()/
// recentRankHistory()/playtimeHistory() accessors.

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
#include "presentation/benchmarks/benchmark_format.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/graphs/playtime_graph_vm.h"
#include "presentation/benchmarks/tracking/benchmark_history_vm.h"
#include "presentation/graphs/common/graph_vm_base.h"
#include "presentation/graphs/common/series/series_model.h"
#include "presentation/graphs/common/axes/value_axis.h"

#include "fake_benchmark_tracking_use_case.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_ui;

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
    // Playtime runs later than the last rank change, and the axis rounds up to a tick.
    snapshot.visiblePlaytime = {{ymd(2024, 3, 5), 600.0}, {ymd(2024, 3, 20), 1200.0}};
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
    EXPECT_DOUBLE_EQ(rank.trailingCarry->y(), 2.5);
}

TEST(BenchmarkHistoryViewModel, AnAnchoredWholeRankHistoryEndsAtItsLastPointNotTheSharedExtent) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.visiblePlaytime = {{ymd(2024, 3, 5), 600.0}, {ymd(2024, 3, 20), 1200.0}};
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
    snapshot.visiblePlaytime = {{ymd(2024, 3, 5), 600.0}, {ymd(2024, 3, 7), 1200.0}};
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
    auto snapshot = snapshotFor(makeUniformFixture(3, {{4, 100}, {5, 200}}, 100));
    snapshot.visiblePlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().min(),
                     static_cast<double>(expectedMidnightMs(ymd(2024, 3, 4))));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->xAxis().max(),
                     static_cast<double>(expectedMidnightMs(ymd(2024, 3, 5))));
}

TEST(BenchmarkHistoryViewModel, NoHistoryYieldsDegenerateCurrentDayRangeAndNoLine) {
    auto fixture = makeTrackableFixture();
    fixture.runs.clear();
    auto snapshot = snapshotFor(fixture);
    snapshot.visiblePlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto nowMs = static_cast<double>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    for (GraphViewModelBase *history : {vm.rankHistory(), vm.recentRankHistory(), vm.playtimeHistory()}) {
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
    auto snapshot = snapshotFor(makeUniformFixture(3, {{4, 25}, {5, 160}, {6, 200}, {7, 300}}, 100));
    snapshot.visiblePlaytime.clear();
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
    auto fixture = makeTrackableFixture();
    fixture.completeness = CompletenessResult{Completeness::Incomplete, {BenchmarkIssue{BenchmarkIssueCode::NoTiers, std::monostate{}}}};
    auto fake = fakeWith(snapshotFor(fixture));
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_TRUE(vm.rankHistory()->series({kValueColumn}).isEmpty());
    EXPECT_FALSE(vm.playtimeHistory()->series({kValueColumn}).isEmpty());
}

namespace {
    Tier coloredTier(const std::string &id, const std::string &name, const BenchmarkColor color) {
        return {TierId{id}, name, color};
    }

    BenchmarkWorkspaceSnapshot snapshotWithTiers(std::vector<Tier> tiers) {
        auto fixture = makeTrackableFixture();
        fixture.definition.tiers = std::move(tiers);
        return snapshotFor(fixture);
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

TEST(BenchmarkHistoryViewModel, PlaytimeSeriesHasNoTierLines) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto series = vm.playtimeHistory()->series({kValueColumn});
    ASSERT_EQ(series.size(), 1);
    EXPECT_TRUE(series.front()->referenceLines.isEmpty());
}

TEST(BenchmarkHistoryViewModel, RankAxisHasATickAtEveryTierEvenWithManyTiers) {
    for (const int tierCount: {3, 5, 7, 12, 13, 25}) {
        auto fake = fakeWith(snapshotFor(makeUniformFixture(tierCount, {{4, static_cast<float>(tierCount)}})));
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
        float peak;
        int shownTiers;
    };
    for (const auto [peak, shownTiers]: {Case{0.25, 1}, Case{1.75, 2}, Case{2.0, 3}, Case{4.5, 5}, Case{5.0, 5}}) {
        auto fake = fakeWith(snapshotFor(makeUniformFixture(5, {{4, peak}, {5, 0}})));
        BenchmarkTrackingViewModel vm{fake};

        for (const auto *history: {vm.rankHistory(), vm.recentRankHistory()}) {
            const auto series = history->series({kValueColumn});
            ASSERT_EQ(series.size(), 1) << peak;
            ASSERT_TRUE(series.front()->yAxis.has_value()) << peak;
            EXPECT_DOUBLE_EQ(series.front()->yAxis->max(), shownTiers) << peak;
            EXPECT_EQ(series.front()->referenceLines.size(), shownTiers) << peak;
            EXPECT_EQ(series.front()->yAxis->formatTick(shownTiers), "Tier " + QString::number(shownTiers)) << peak;
        }
        const auto recent = vm.recentRankHistory()->series({kValueColumn});
        ASSERT_EQ(recent.front()->points.size(), 2);
        EXPECT_LT(recent.front()->points.back().y(), recent.front()->points.front().y());
    }
}

TEST(BenchmarkHistoryViewModel, CanvasDrawsAColourlessSeriesInItsForegroundColour) {
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
    series.front()->setColor(QColor());
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
    }
}

TEST(BenchmarkHistoryViewModel, AWindowChangeRelabelsPlaytimeWithoutRebuildingTheWholeWorkspace) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    QSignalSpy changed(&vm, &BenchmarkTrackingViewModel::changed);
    QSignalSpy renamed(vm.playtimeHistory(), &BenchmarkHistoryViewModel::metricNameChanged);

    fake->snap.playtime.windowDays = 10;
    fake->snap.visiblePlaytime = {{ymd(2024, 3, 4), 1000.0}, {ymd(2024, 3, 6), 500.0}};
    fake->publishPlaytime();

    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(renamed.count(), 1);
    EXPECT_EQ(vm.playtimeHistory()->metricName(), PlaytimeGraphViewModel::titleFor(10));
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->points.front().y(), 1000.0);
}

TEST(BenchmarkHistoryViewModel, ADatePlaytimeRangeSpansItsIntervalWhileTheRankAxisKeepsTheDefaultExtent) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.visiblePlaytime = {{ymd(2024, 3, 4), 600.0}, {ymd(2024, 3, 12), 1200.0}};
    snapshot.playtime.fullExtent = DayInterval{ymd(2024, 3, 4), ymd(2024, 3, 12)};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};
    const auto rankMin = vm.rankHistory()->xAxis().min();
    const auto rankMax = vm.rankHistory()->xAxis().max();

    fake->snap.playtime.interval = DayInterval{ymd(2024, 3, 10), ymd(2024, 3, 12)};
    fake->snap.visiblePlaytime = {{ymd(2024, 3, 12), 1200.0}};
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
    snapshot.visiblePlaytime.clear();
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
    BenchmarkWorkspaceSnapshot rankRangeSnapshot(const std::optional<DayInterval> interval,
                                                  BenchmarkFixture fixture = makeUniformFixture(
                                                      3, {{4, 100}, {6, 200}, {10, 250}}, 100)) {
        auto snapshot = snapshotFor(fixture);
        snapshot.visiblePlaytime = {{ymd(2024, 3, 2), 600.0}, {ymd(2024, 3, 20), 1200.0}};
        snapshot.playtime.fullExtent = DayInterval{ymd(2024, 3, 2), ymd(2024, 3, 20)};
        snapshot.rank.interval = interval;
        return snapshot;
    }

    const SeriesModel *valueSeries(const BenchmarkHistoryViewModel *history) {
        const auto series = history->series({kValueColumn});
        return series.isEmpty() ? nullptr : series.front();
    }

    const SeriesModel *rankSeries(const BenchmarkTrackingViewModel &vm) { return valueSeries(vm.rankHistory()); }

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
    EXPECT_GT(countPixelsNear(image, plot.toAlignedRect(), rank->color()), 0) << "the carried line was not drawn";
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
    auto fake = fakeWith(snapshotFor(makeUniformFixture(6, {{4, 1}, {8, 4.5}})));
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
    fake->snap.visiblePlaytime = {};
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
    EXPECT_TRUE(history->noPointsInRange());
    EXPECT_TRUE(history->property("noPointsInRange").toBool());
}

TEST(BenchmarkHistoryContent, ALineModeDrawsTheCarriedRankSoThereIsNothingToExplain) {
    auto fake = fakeWith(carriedOnlySnapshot());
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_FALSE(vm.rankHistory()->noPointsInRange());
    fake->rankPresentation.appearance = {ConnectionMode::Straight, false};
    fake->publishPresentation();
    EXPECT_FALSE(vm.rankHistory()->noPointsInRange());
}

TEST(BenchmarkHistoryContent, SwitchingTheLineModeAnnouncesTheContentStateChange) {
    auto fake = fakeWith(carriedOnlySnapshot());
    BenchmarkTrackingViewModel vm{fake};
    const QSignalSpy spy(vm.rankHistory(), &BenchmarkHistoryViewModel::contentStateChanged);

    fake->rankPresentation.appearance = {ConnectionMode::PointsOnly, false};
    fake->publishPresentation();
    EXPECT_TRUE(vm.rankHistory()->noPointsInRange());
    EXPECT_GE(spy.count(), 1);

    fake->rankPresentation.appearance = {ConnectionMode::Smooth, false};
    fake->publishPresentation();
    EXPECT_FALSE(vm.rankHistory()->noPointsInRange());
}

TEST(BenchmarkHistoryContent, PointsOnlyWithImprovementsInTheRangeHasPointsToShow) {
    auto fake = fakeWith(rankRangeSnapshot(DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 12)}));
    fake->rankPresentation.appearance = {ConnectionMode::PointsOnly, false};
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.rankHistory()->observationCount(), 2);
    EXPECT_FALSE(vm.rankHistory()->noPointsInRange());
}

TEST(BenchmarkHistoryContent, PointsOnlyBeforeAnyRankIsEstablishedIsAnEmptyRangeNotMissingImprovements) {
    auto fake = fakeWith(rankRangeSnapshot(DayInterval{ymd(2024, 3, 1), ymd(2024, 3, 3)}));
    fake->rankPresentation.appearance = {ConnectionMode::PointsOnly, false};
    BenchmarkTrackingViewModel vm{fake};
    const auto *history = vm.rankHistory();

    EXPECT_FALSE(history->hasCarriedState());
    EXPECT_FALSE(history->noPointsInRange());
    EXPECT_TRUE(history->noObservationsInRange());
}

// EDGE-04: an anchor with no contributing run is the ordinary no-history state, even when stale
// the benchmark has a full rank history.
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
    snapshot.visiblePlaytime.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};
    const auto *playtime = vm.playtimeHistory();

    EXPECT_TRUE(playtime->historyAvailable());
    EXPECT_TRUE(playtime->noObservationsInRange());
    EXPECT_FALSE(playtime->hasData());
    EXPECT_FALSE(playtime->noPointsInRange());
}

// ---- recent-average rank --------------------------------------------------------------------

namespace {
    const SeriesModel *recentSeries(const BenchmarkTrackingViewModel &vm) {
        return valueSeries(vm.recentRankHistory());
    }

    // Play days on 4, 6 and 10 March 2024, dipping on the 6th; playtime spans 2 to 20 March.
    BenchmarkWorkspaceSnapshot recentRangeSnapshot(const std::optional<DayInterval> interval) {
        auto snapshot = rankRangeSnapshot(std::nullopt, makeUniformFixture(
            3, {{4, 200}, {6, 100}, {10, 300}}, 100));
        snapshot.recentRank.interval = interval;
        return snapshot;
    }
}

TEST(BenchmarkRecentRankHistory, IsItsOwnNamedTierGraphOnItsOwnAxis) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    auto *recent = vm.recentRankHistory();

    EXPECT_NE(&recent->xAxis(), &vm.rankHistory()->xAxis());
    EXPECT_NE(&recent->xAxis(), &vm.playtimeHistory()->xAxis());
    EXPECT_EQ(recent->metricName(), "Recent average rank");
    const auto *series = recentSeries(vm);
    ASSERT_NE(series, nullptr);
    EXPECT_EQ(series->name(), "Recent average rank");
    ASSERT_TRUE(series->yAxis);
    EXPECT_EQ(series->yAxis->formatTick(1.0), "Bronze");
    EXPECT_EQ(series->delegate.axisLabel(1.2), describeAverageRank(1.2, {"Bronze", "Silver", "Gold"}));
}

TEST(BenchmarkRecentRankHistory, KeepsTheFallsOfTheRecentAverage) {
    auto fake = fakeWith(snapshotFor(makeUniformFixture(3, {{4, 100}, {5, 200}, {6, 0}}, 100)));
    BenchmarkTrackingViewModel vm{fake};

    const auto *series = recentSeries(vm);
    ASSERT_NE(series, nullptr);
    ASSERT_EQ(series->points.size(), 3);
    EXPECT_EQ(series->points[1], QPointF(ms(ymd(2024, 3, 5)), 1.5));
    EXPECT_EQ(series->points[2], QPointF(ms(ymd(2024, 3, 6)), 1.0));
}

TEST(BenchmarkRecentRankHistory, ItsTierAxisFollowsItsOwnPeakNotThePersonalBests) {
    auto snapshot = snapshotFor(makeUniformFixture(5, {{4, 1}, {6, 5}}, 1));
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_DOUBLE_EQ(recentSeries(vm)->yAxis->max(), 4.0);
    EXPECT_EQ(recentSeries(vm)->referenceLines.size(), 4);
    EXPECT_DOUBLE_EQ(rankSeries(vm)->yAxis->max(), 5.0);
}

TEST(BenchmarkRecentRankHistory, ARangeCarriesTheHeldValueInAndOut) {
    auto fake = fakeWith(recentRangeSnapshot(DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 8)}));
    BenchmarkTrackingViewModel vm{fake};

    const auto *series = recentSeries(vm);
    ASSERT_NE(series, nullptr);
    ASSERT_EQ(series->points.size(), 1);
    EXPECT_EQ(series->points[0], QPointF(ms(ymd(2024, 3, 6)), 1.5));
    ASSERT_TRUE(series->leadingCarry);
    EXPECT_EQ(*series->leadingCarry, QPointF(ms(ymd(2024, 3, 5)), 2.0));
    ASSERT_TRUE(series->trailingCarry);
    EXPECT_EQ(*series->trailingCarry, QPointF(ms(ymd(2024, 3, 8)), 1.5));
}

TEST(BenchmarkRecentRankHistory, ItsRangeMovesOnlyItsOwnAxisAndSeries) {
    auto fake = fakeWith(recentRangeSnapshot(std::nullopt));
    BenchmarkTrackingViewModel vm{fake};
    const auto rankMin = vm.rankHistory()->xAxis().min();
    const auto rankMax = vm.rankHistory()->xAxis().max();
    const QList<QPointF> rankPoints{{ms(ymd(2024, 3, 4)), 2.0}, {ms(ymd(2024, 3, 10)), 3.0}};
    EXPECT_EQ(rankSeries(vm)->points, rankPoints);
    const QSignalSpy changed(&vm, &BenchmarkTrackingViewModel::changed);

    fake->snap.recentRank.interval = DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 12)};
    fake->publishRank(GraphKey::BenchmarkRecentRank);

    EXPECT_EQ(changed.count(), 0);
    EXPECT_DOUBLE_EQ(vm.recentRankHistory()->xAxis().min(), ms(ymd(2024, 3, 5)));
    EXPECT_DOUBLE_EQ(vm.recentRankHistory()->xAxis().max(), ms(ymd(2024, 3, 12)));
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().min(), rankMin);
    EXPECT_DOUBLE_EQ(vm.rankHistory()->xAxis().max(), rankMax);
    EXPECT_EQ(rankSeries(vm)->points, rankPoints);
    EXPECT_EQ(recentSeries(vm)->points.size(), 2);
}

TEST(BenchmarkRecentRankHistory, ItsHistoryWidensTheSharedDefaultExtent) {
    auto snapshot = snapshotFor(makeUniformFixture(3, {{4, 100}, {9, 50}}, 100));
    snapshot.visiblePlaytime = {{ymd(2024, 3, 5), 600.0}};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    for (GraphViewModelBase *history: {vm.rankHistory(), vm.recentRankHistory(), vm.playtimeHistory()}) {
        EXPECT_DOUBLE_EQ(history->xAxis().min(), ms(ymd(2024, 3, 4)));
        EXPECT_DOUBLE_EQ(history->xAxis().max(), ms(ymd(2024, 3, 9)));
    }
}

TEST(BenchmarkRecentRankHistory, IsAbsentForAnIncompleteDefinition) {
    auto fixture = makeTrackableFixture();
    fixture.completeness = CompletenessResult{Completeness::Incomplete, {}};
    auto fake = fakeWith(snapshotFor(fixture));
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_FALSE(vm.recentRankHistory()->hasData());
    EXPECT_TRUE(vm.recentRankHistory()->series({kValueColumn}).isEmpty());
}

TEST(BenchmarkRecentRankHistory, ExplainsAnEmptyPointsOnlyRangeInTermsOfPlayDays) {
    auto fake = fakeWith(recentRangeSnapshot(DayInterval{ymd(2024, 3, 11), ymd(2024, 3, 13)}));
    fake->recentRankPresentation.appearance = {ConnectionMode::PointsOnly, false};
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_TRUE(vm.recentRankHistory()->noPointsInRange());
    EXPECT_TRUE(vm.recentRankHistory()->noPointsText().contains("no play days"));
    EXPECT_TRUE(vm.rankHistory()->noPointsText().contains("no improvement points"));
}

TEST(BenchmarkRecentRankHistory, TakesItsOwnCommittedStyle) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    const QSignalSpy rankStyle(vm.rankHistory(), &GraphViewModelBase::styleChanged);

    fake->recentRankPresentation.appearance = {ConnectionMode::Straight, true};
    fake->recentRankPresentation.seriesStyles["rank"] = {RgbaColor{3, 4, 5, 255}, 7.0};
    fake->publishPresentation();

    EXPECT_EQ(vm.recentRankHistory()->property("connectionMode").toInt(), static_cast<int>(ConnectionMode::Straight));
    EXPECT_EQ(recentSeries(vm)->color(), QColor(3, 4, 5));
    EXPECT_DOUBLE_EQ(recentSeries(vm)->lineWidth, 7.0);
    EXPECT_EQ(rankStyle.count(), 0);
}

TEST(BenchmarkRecentRankHistory, TakesItsOwnRankAxisPolicy) {
    auto fake = fakeWith(snapshotFor(makeUniformFixture(5, {{4, 300}, {5, 340}}, 100)));
    BenchmarkTrackingViewModel vm{fake};
    const auto rankMax = rankSeries(vm)->yAxis->max();
    const QSignalSpy rankBounds(vm.rankHistory(), &GraphViewModelBase::boundsChanged);

    fake->recentRankPresentation.axisPolicies["rank"] = FitVisibleData{};
    fake->publishPresentation();

    EXPECT_GT(recentSeries(vm)->yAxis->min(), 0.0);
    EXPECT_DOUBLE_EQ(rankSeries(vm)->yAxis->min(), 0.0);
    EXPECT_DOUBLE_EQ(rankSeries(vm)->yAxis->max(), rankMax);
    EXPECT_EQ(rankBounds.count(), 0);
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
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimeFitsFixesAndRestoresItsAxisInMinutes) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime);
    vm.update(secondsContent({1800.0, 4200.0}), true, {});
    const double defaultMin = historyAxis(vm).min();

    vm.setAxisPolicies({{"playtime", FitVisibleData{}}});
    ValueAxis fitted({}, ValueDelegate::secondsToMinutes());
    fitted.setRange(30.0, 70.0);
    expectSameAxis(historyAxis(vm), fitted);

    vm.setAxisPolicies({{"playtime", FixedBounds{10.0, 45.0}}});
    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 10.0);
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 45.0);
    EXPECT_EQ(historyAxis(vm).formatTick(45.0), "45 min");

    vm.setAxisPolicies({});
    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), defaultMin);
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimePolicySurvivesFurtherUpdates) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime);
    vm.setAxisPolicies({{"playtime", FixedBounds{10.0, 45.0}}});

    vm.update(secondsContent({600.0}), true, {});
    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 10.0);
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 45.0);

    vm.update(secondsContent({600.0, 7200.0}), true, {});
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 45.0);
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimeFitOfASingleDayUsesTheFallbackSpan) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime);
    vm.setAxisPolicies({{"playtime", FitVisibleData{}}});

    vm.update(secondsContent({1800.0}), true, {});

    ValueAxis expected({}, ValueDelegate::secondsToMinutes());
    expected.setRange(30.0, 30.0);
    expectSameAxis(historyAxis(vm), expected);
}

TEST(BenchmarkHistoryAxisPolicy, PlaytimePolicyChangesRepaintOnlyWhenTheyChangeSomething) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::RollingPlaytime);
    vm.update(secondsContent({1800.0}), true, {});
    const QSignalSpy bounds(&vm, &GraphViewModelBase::boundsChanged);

    vm.setAxisPolicies({{"playtime", FitVisibleData{}}});
    vm.setAxisPolicies({{"playtime", FitVisibleData{}}});

    EXPECT_EQ(bounds.count(), 1);
}

// ── Rank Y-axis policy and tier rounding ─────────────────────────────────────────────────────────

namespace {
    const std::vector<RankTier> kFiveTiers{{"Bronze", QColor("brown")}, {"Silver", QColor("gray")},
                                           {"Gold", QColor("gold")}, {"Platinum", QColor("cyan")},
                                           {"Diamond", QColor("blue")}};

    BenchmarkHistoryViewModel::Content rankContent(const QList<qreal> &ranks, const qreal peak = 5.0) {
        BenchmarkHistoryViewModel::Content content;
        content.historyAvailable = true;
        content.fullHistoryPeak = peak;
        qreal x = 0.0;
        for (const qreal rank: ranks) content.observations.append(QPointF(x++, rank));
        return content;
    }

    GraphPresentation rankPresentation(const TierRounding rounding, AxisPolicy policy = FitVisibleData{}) {
        auto presentation = defaultGraphPresentation(GraphKey::BenchmarkRank);
        presentation.axisPolicies["rank"] = std::move(policy);
        presentation.tierRounding = rounding;
        return presentation;
    }

    QStringList tickLabels(const ValueAxis &axis) {
        QStringList labels;
        for (const qreal tick: axis.ticks()) labels.append(axis.formatTick(tick));
        return labels;
    }
}

TEST(BenchmarkRankAxis, ByDefaultTheAxisFitsTheVisibleRanksToTheirEnclosingTiers) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
    vm.update(rankContent({3.2, 3.6}), true, kFiveTiers);

    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 3.0);
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 4.0);
    EXPECT_EQ(tickLabels(historyAxis(vm)), (QStringList{"Gold", "Platinum"}));
}

TEST(BenchmarkRankAxis, EachTierRoundingBoundsTheVisibleRanksAsSpecified) {
    struct Case {
        QList<qreal> ranks;
        TierRounding rounding;
        qreal min;
        qreal max;
    };
    const Case cases[] = {
        {{3.4, 3.4}, TierRounding::Both, 3.0, 4.0},
        // A flat line on a whole tier widens a tier each way rather than collapsing the axis.
        {{3.0, 3.0}, TierRounding::Both, 2.0, 4.0},
        // A flat line is stepped as if it spanned a whole tier: 20 % steps.
        {{3.4, 3.4}, TierRounding::Off, 3.2, 3.6},
        {{3.37, 3.82}, TierRounding::Off, 3.3, 3.9},
        // The bottom sits on a tick, so padding moves it one 20 % step further out.
        {{3.0, 3.6}, TierRounding::Above, 2.8, 4.0},
        {{3.0, 3.6}, TierRounding::Below, 3.0, 3.8},
        {{0.0, 0.0}, TierRounding::Off, 0.0, 0.2},
        {{4.6, 5.0}, TierRounding::Off, 4.5, 5.0},
    };
    for (const auto &[ranks, rounding, min, max]: cases) {
        SCOPED_TRACE(testing::Message() << tierRoundingName(rounding) << " " << ranks.front() << "-" << ranks.back());
        BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
        vm.setPresentation(rankPresentation(rounding));
        vm.update(rankContent(ranks), true, kFiveTiers);
        EXPECT_NEAR(historyAxis(vm).min(), min, 1e-9);
        EXPECT_NEAR(historyAxis(vm).max(), max, 1e-9);
    }
}

TEST(BenchmarkRankAxis, ACarriedRankWidensTheFit) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
    auto content = rankContent({3.2});
    content.leadingCarry = QPointF(-1.0, 2.5);
    vm.update(content, true, kFiveTiers);

    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 2.0);
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 4.0);
}

TEST(BenchmarkRankAxis, StartingAtUnrankedSpansEveryShownTierWhateverIsVisible) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
    vm.setPresentation(rankPresentation(TierRounding::Off, StartAtZero{}));
    vm.update(rankContent({3.2, 3.6}, 3.6), true, kFiveTiers);

    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 0.0);
    EXPECT_DOUBLE_EQ(historyAxis(vm).max(), 4.0);
    EXPECT_EQ(tickLabels(historyAxis(vm)), (QStringList{"Unranked", "Bronze", "Silver", "Gold", "Platinum"}));
}

TEST(BenchmarkRankAxis, PaddedTicksNameTheTierEachBandIsProgressToward) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
    vm.setPresentation(rankPresentation(TierRounding::Off));

    vm.update(rankContent({3.37, 3.82}), true, kFiveTiers);
    EXPECT_EQ(tickLabels(historyAxis(vm)),
              (QStringList{"30% to Platinum", "40%", "50%", "60%", "70%", "80%", "90%"}));

    vm.update(rankContent({3.7, 4.3}), true, kFiveTiers);
    EXPECT_EQ(tickLabels(historyAxis(vm)), (QStringList{"60% to Platinum", "80%", "Platinum", "20% to Diamond", "40%"}));

    vm.update(rankContent({0.3, 0.5}), true, kFiveTiers);
    EXPECT_EQ(tickLabels(historyAxis(vm)), (QStringList{"20% to Bronze", "30%", "40%", "50%", "60%"}));
}

TEST(BenchmarkRankAxis, RoundingBothWaysKeepsOneTickPerTier) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
    vm.update(rankContent({1.2, 3.6}), true, kFiveTiers);

    EXPECT_EQ(historyAxis(vm).ticks(), (QList<qreal>{1.0, 2.0, 3.0, 4.0}));
    EXPECT_EQ(tickLabels(historyAxis(vm)), (QStringList{"Bronze", "Silver", "Gold", "Platinum"}));
}

// The canvas drops tier lines outside the axis, so the series keeps every shown tier's line in
// every mode; a tier-rounded edge lands exactly on its tier's line.
TEST(BenchmarkRankAxis, TierLinesAreTheSameInEveryModeAndATierRoundedEdgeIsOne) {
    QList<qreal> expected;
    for (int tier = 1; tier <= 5; ++tier) expected.append(tier);
    for (const auto &presentation: {rankPresentation(TierRounding::Both), rankPresentation(TierRounding::Off),
                                    rankPresentation(TierRounding::Both, StartAtZero{})}) {
        BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
        vm.setPresentation(presentation);
        vm.update(rankContent({3.2, 3.6}), true, kFiveTiers);
        QList<qreal> values;
        for (const auto &line: vm.series({kValueColumn}).front()->referenceLines) values.append(line.value);
        EXPECT_EQ(values, expected);
    }

    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
    vm.setPresentation(rankPresentation(TierRounding::Both));
    vm.update(rankContent({3.2, 3.6}), true, kFiveTiers);
    EXPECT_TRUE(expected.contains(historyAxis(vm).min()));
    EXPECT_TRUE(expected.contains(historyAxis(vm).max()));
}

TEST(BenchmarkRankAxis, APolicyOrRoundingChangeRebindsTheAxisOnlyWhenItChangesSomething) {
    BenchmarkHistoryViewModel vm(BenchmarkHistoryViewModel::AverageRank);
    vm.update(rankContent({3.37, 3.82}), true, kFiveTiers);
    const QSignalSpy bounds(&vm, &GraphViewModelBase::boundsChanged);

    vm.setPresentation(rankPresentation(TierRounding::Off));
    EXPECT_EQ(bounds.count(), 1);
    EXPECT_NEAR(historyAxis(vm).min(), 3.3, 1e-9);

    vm.setPresentation(rankPresentation(TierRounding::Off, StartAtZero{}));
    EXPECT_EQ(bounds.count(), 2);
    EXPECT_DOUBLE_EQ(historyAxis(vm).min(), 0.0);

    auto restyled = rankPresentation(TierRounding::Off, StartAtZero{});
    restyled.seriesStyles["rank"] = {RgbaColor{1, 2, 3, 255}, std::nullopt};
    vm.setPresentation(restyled);
    vm.setAxisPolicies({{"rank", StartAtZero{}}});
    EXPECT_EQ(bounds.count(), 2);
}


// ---- committed style ----------------------------------------------------------------------

TEST(BenchmarkHistoryStyle, WithNothingSavedBothGraphsUseTheirDefaults) {
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
    ASSERT_TRUE(rankStyle.color.has_value());
    EXPECT_EQ(vm.rankHistory()->series({kValueColumn}).front()->color(),
              QColor(rankStyle.color->red, rankStyle.color->green, rankStyle.color->blue, rankStyle.color->alpha));
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
    const QString nameBefore = vm.rankHistory()->metricName();

    fake->rankPresentation.seriesStyles["rank"] = {RgbaColor{12, 34, 56, 255}, std::nullopt};
    fake->publishPresentation();
    fake->publishRank();

    const auto *rank = vm.rankHistory()->series({kValueColumn}).front();
    EXPECT_EQ(rank->color(), QColor(12, 34, 56));
    EXPECT_EQ(rank->name(), nameBefore);
    EXPECT_EQ(vm.rankHistory()->metricName(), nameBefore);
    EXPECT_EQ(tierColours(), tiersBefore);
}

TEST(BenchmarkHistoryStyle, TheStyleSurvivesRebuildsAndBenchmarkSelectionChanges) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    fake->rankPresentation.seriesStyles["rank"] = {RgbaColor{1, 1, 1, 255}, 5.0};
    fake->playtimePresentation.appearance = {ConnectionMode::Straight, true};
    fake->publishPresentation();

    auto fixture = makeTrackableFixture();
    fixture.definition.id = BenchmarkId{"b2"};
    fixture.runs = {runOn(ymd(2025, 1, 1), "h1", 600, 1800),
                    runOn(ymd(2025, 1, 1), "h1", 500),
                    runOn(ymd(2025, 1, 9), "h1", 800, 900),
                    runOn(ymd(2025, 1, 9), "h2", 800, 961)};
    fake->show(std::move(fixture));
    fake->snap.visiblePlaytime = {{ymd(2025, 1, 1), 60.0}, {ymd(2025, 1, 9), 120.0}};
    fake->publish();
    fake->publishPlaytime();
    fake->publishRank();

    const auto *rank = vm.rankHistory()->series({kValueColumn}).front();
    EXPECT_EQ(rank->color(), QColor(1, 1, 1));
    EXPECT_DOUBLE_EQ(rank->lineWidth, 5.0);
    EXPECT_EQ(vm.playtimeHistory()->property("connectionMode").toInt(), static_cast<int>(ConnectionMode::Straight));
    EXPECT_TRUE(vm.playtimeHistory()->property("pointMarkers").toBool());
}

TEST(BenchmarkHistoryStyle, EachGraphTakesOnlyItsOwnAxisPolicy) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    const auto rankAxisMax = vm.rankHistory()->series({kValueColumn}).front()->yAxis->max();
    const auto playtimeAxisMax = vm.playtimeHistory()->series({kValueColumn}).front()->yAxis->max();

    fake->rankPresentation.axisPolicies["playtime"] = FixedBounds{5.0, 45.0};
    fake->playtimePresentation.axisPolicies["rank"] = FitVisibleData{};
    fake->playtimePresentation.tierRounding = TierRounding::Off;
    fake->publishPresentation();
    EXPECT_DOUBLE_EQ(vm.rankHistory()->series({kValueColumn}).front()->yAxis->max(), rankAxisMax);
    EXPECT_DOUBLE_EQ(vm.playtimeHistory()->series({kValueColumn}).front()->yAxis->max(), playtimeAxisMax);

    fake->playtimePresentation.axisPolicies["playtime"] = FixedBounds{5.0, 45.0};
    fake->publishPresentation();
    const auto &playtimeAxis = *vm.playtimeHistory()->series({kValueColumn}).front()->yAxis;
    EXPECT_DOUBLE_EQ(playtimeAxis.min(), 5.0);
    EXPECT_DOUBLE_EQ(playtimeAxis.max(), 45.0);
    EXPECT_DOUBLE_EQ(vm.rankHistory()->series({kValueColumn}).front()->yAxis->max(), rankAxisMax);
}

TEST(BenchmarkHistoryViewModel, PlaytimeGraphPlotsTheSnapshotsVisibleSeriesNotTheBenchmarks) {
    auto snapshot = makeTrackableSnapshot();
    ASSERT_GT(snapshot.benchmark->rollingPlaytime(1).size(), 1u);
    const auto day = ymd(2024, 3, 6);
    snapshot.visiblePlaytime = {{day, 120}};
    BenchmarkTrackingViewModel vm{fakeWith(std::move(snapshot))};

    const auto *series = valueSeries(vm.playtimeHistory());
    ASSERT_NE(series, nullptr);
    ASSERT_EQ(series->points.size(), 1);
    EXPECT_EQ(series->points.front(), QPointF(ms(day), 120));
}

// ---- run axis -------------------------------------------------------------------------------------

namespace {
    std::shared_ptr<FakeBenchmarkTrackingUseCase> onRunAxis(std::shared_ptr<FakeBenchmarkTrackingUseCase> fake) {
        fake->rankPresentation.xAxis = std::string(kRunXAxis);
        fake->recentRankPresentation.xAxis = std::string(kRunXAxis);
        return fake;
    }

    std::vector<double> xs(const SeriesModel *series) {
        std::vector<double> result;
        if (series)
            for (const auto &point: series->points) result.push_back(point.x());
        return result;
    }

    // makeTrackableFixture's member runs are 1 (Mar 4), 2 (Mar 5), 3 and 4 (Mar 6); runs 1, 3 and 4 set PBs.
}

TEST(BenchmarkRunAxis, PointsSitAtTheirBenchmarkRunNumbers) {
    auto fake = onRunAxis(fakeWith(makeTrackableSnapshot()));
    BenchmarkTrackingViewModel vm{fake};

    const auto *history = vm.rankHistory();
    EXPECT_TRUE(history->runAxisShown());
    const auto *rank = valueSeries(history);
    ASSERT_NE(rank, nullptr);
    const auto &domainHistory = fake->benchmark()->averageRankHistory();
    ASSERT_EQ(rank->points.size(), static_cast<qsizetype>(domainHistory.size()));
    for (std::size_t index = 0; index < domainHistory.size(); ++index) {
        EXPECT_EQ(rank->points[static_cast<qsizetype>(index)].x(), domainHistory[index].benchmarkRun);
        EXPECT_DOUBLE_EQ(rank->points[static_cast<qsizetype>(index)].y(), domainHistory[index].averageRank);
    }
    EXPECT_EQ(xs(rank), (std::vector<double>{1, 3, 4}));
    EXPECT_EQ(history->xAxis().min(), 1);
    EXPECT_EQ(history->xAxis().max(), 4);
}

TEST(BenchmarkRunAxis, TheRecentGraphHasAPointForEveryMemberRun) {
    auto fake = onRunAxis(fakeWith(makeTrackableSnapshot()));
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(xs(valueSeries(vm.recentRankHistory())), (std::vector<double>{1, 2, 3, 4}));
}

TEST(BenchmarkRunAxis, ADateRangeSpansTheRunsOfItsDays) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.rank.interval = DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 9)};
    auto fake = onRunAxis(fakeWith(snapshot));
    BenchmarkTrackingViewModel vm{fake};

    const auto &axis = vm.rankHistory()->xAxis();
    EXPECT_EQ(axis.min(), 2);
    EXPECT_EQ(axis.max(), 4);
    EXPECT_EQ(xs(valueSeries(vm.rankHistory())), (std::vector<double>{3, 4}));
    EXPECT_EQ(axis.calendarRangeMs(),
              std::optional(std::pair{expectedMidnightMs(ymd(2024, 3, 5)), expectedMidnightMs(ymd(2024, 3, 9))}));
}

// R24: the run axis has nothing to span where the range holds no run, so it shows the empty state
// even though the calendar axis carries the held rank across those days.
TEST(BenchmarkRunAxis, ARangeWithoutRunsDrawsNothingWhereTheCalendarAxisWouldCarry) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.rank.interval = DayInterval{ymd(2024, 3, 10), ymd(2024, 3, 12)};
    auto calendarFake = fakeWith(snapshot);
    auto runFake = onRunAxis(fakeWith(snapshot));
    BenchmarkTrackingViewModel calendar{calendarFake};
    BenchmarkTrackingViewModel runs{runFake};

    auto *onCalendar = qobject_cast<BenchmarkHistoryViewModel *>(calendar.rankHistory());
    auto *onRuns = qobject_cast<BenchmarkHistoryViewModel *>(runs.rankHistory());
    ASSERT_TRUE(onCalendar->hasData());
    ASSERT_TRUE(onCalendar->hasCarriedState());
    EXPECT_FALSE(onRuns->hasData());
    EXPECT_TRUE(onRuns->historyAvailable());
    EXPECT_TRUE(onRuns->noObservationsInRange());
    EXPECT_TRUE(onRuns->runAxisShown());
}

TEST(BenchmarkRunAxis, ALevelIsCarriedInFromTheFirstRunAndOutToTheLastRun) {
    const auto &pbHistory = makeTrackableFixture().evaluate().averageRankHistory();
    ASSERT_EQ(pbHistory.front().benchmarkRun, 1);
    const double firstLevel = pbHistory.front().averageRank;

    auto carriedIn = makeTrackableSnapshot();
    carriedIn.rank.interval = DayInterval{ymd(2024, 3, 5), ymd(2024, 3, 6)};
    auto inFake = onRunAxis(fakeWith(carriedIn));
    BenchmarkTrackingViewModel inVm{inFake};
    const auto *in = valueSeries(inVm.rankHistory());
    ASSERT_NE(in, nullptr);
    ASSERT_TRUE(in->leadingCarry);
    EXPECT_EQ(in->leadingCarry->x(), 2);
    EXPECT_DOUBLE_EQ(in->leadingCarry->y(), firstLevel);
    EXPECT_FALSE(in->trailingCarry) << "run 4 is both the last run and a point";

    auto carriedOut = makeTrackableSnapshot();
    carriedOut.rank.interval = DayInterval{ymd(2024, 3, 4), ymd(2024, 3, 5)};
    auto outFake = onRunAxis(fakeWith(carriedOut));
    BenchmarkTrackingViewModel outVm{outFake};
    const auto *out = valueSeries(outVm.rankHistory());
    ASSERT_NE(out, nullptr);
    EXPECT_FALSE(out->leadingCarry);
    ASSERT_TRUE(out->trailingCarry);
    EXPECT_EQ(out->trailingCarry->x(), 2);
    EXPECT_DOUBLE_EQ(out->trailingCarry->y(), firstLevel);
}

TEST(BenchmarkRunAxis, AllHistorySpansEveryMemberRunWhateverThePlaytimeExtent) {
    auto fixture = makeTrackableFixture();
    fixture.runs.push_back(runOn(ymd(2024, 3, 7), "h1", 100));
    auto snapshot = snapshotFor(fixture);
    snapshot.visiblePlaytime = {{ymd(2024, 3, 1), 600.0}, {ymd(2024, 3, 30), 1200.0}};
    snapshot.playtime.fullExtent = DayInterval{ymd(2024, 3, 1), ymd(2024, 3, 30)};
    auto fake = onRunAxis(fakeWith(snapshot));
    BenchmarkTrackingViewModel vm{fake};

    for (const auto *history: {vm.rankHistory(), vm.recentRankHistory()}) {
        EXPECT_EQ(history->xAxis().min(), 1);
        EXPECT_EQ(history->xAxis().max(), 5);
    }
    const auto *rank = valueSeries(vm.rankHistory());
    ASSERT_NE(rank, nullptr);
    ASSERT_TRUE(rank->trailingCarry) << "the PB is held through the run since it was set";
    EXPECT_EQ(rank->trailingCarry->x(), 5);
}

TEST(BenchmarkRunAxis, AnAnchoredRangeEndsAtTheAnchorDaysLastRun) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.recentRank.anchoredEnd = ymd(2024, 3, 5);
    auto fake = onRunAxis(fakeWith(snapshot));
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.recentRankHistory()->xAxis().min(), 1);
    EXPECT_EQ(vm.recentRankHistory()->xAxis().max(), 2);
    EXPECT_EQ(xs(valueSeries(vm.recentRankHistory())), (std::vector<double>{1, 2}));
}

// The run axis draws every run, so a day whose recent average rose and fell again reaches higher
// than that day's last value, which is all the calendar axis plots.
TEST(BenchmarkRunAxis, TheRecentTierScaleCoversItsHighestRunNotOnlyDayEnds) {
    auto fixture = makeUniformFixture(3, {}, 100);
    fixture.runs = {ksv::tests_support::benchmarkFact("u", expectedMidnightMs(ymd(2024, 3, 4)) + 1'000, 250.0F),
                    ksv::tests_support::benchmarkFact("u", expectedMidnightMs(ymd(2024, 3, 4)) + 2'000, 50.0F)};
    const auto snapshot = snapshotFor(fixture);
    auto calendarFake = fakeWith(snapshot);
    auto runFake = onRunAxis(fakeWith(snapshot));
    BenchmarkTrackingViewModel calendar{calendarFake};
    BenchmarkTrackingViewModel runs{runFake};

    const auto *onCalendar = valueSeries(qobject_cast<BenchmarkHistoryViewModel *>(calendar.recentRankHistory()));
    const auto *onRuns = valueSeries(qobject_cast<BenchmarkHistoryViewModel *>(runs.recentRankHistory()));
    ASSERT_NE(onCalendar, nullptr);
    ASSERT_NE(onRuns, nullptr);
    EXPECT_EQ(onRuns->referenceLines.size(), 3);
    EXPECT_LT(onCalendar->referenceLines.size(), onRuns->referenceLines.size());
}

// R16: the axis is a per-graph preference; switching it must not redraw the other graphs.
TEST(BenchmarkRunAxis, SwitchingOneGraphsAxisRebuildsOnlyThatGraph) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    const QSignalSpy rank(vm.rankHistory(), &GraphViewModelBase::dataUpdated);
    const QSignalSpy recent(vm.recentRankHistory(), &GraphViewModelBase::dataUpdated);
    const QSignalSpy playtime(vm.playtimeHistory(), &GraphViewModelBase::dataUpdated);

    fake->rankPresentation.xAxis = std::string(kRunXAxis);
    fake->publishPresentation();

    EXPECT_TRUE(vm.rankHistory()->runAxisShown());
    EXPECT_EQ(rank.count(), 1);
    EXPECT_EQ(recent.count(), 0);
    EXPECT_EQ(playtime.count(), 0);
    EXPECT_FALSE(vm.recentRankHistory()->runAxisShown());

    const int reads = fake->snapshotCalls;
    fake->rankPresentation.appearance.connection = ConnectionMode::Straight;
    fake->publishPresentation();
    EXPECT_EQ(fake->snapshotCalls, reads) << "a style change that leaves the axis alone reads nothing";
}

TEST(BenchmarkRunAxis, RunAxisShownFollowsTheCommittedAxis) {
    auto fake = onRunAxis(fakeWith(makeTrackableSnapshot()));
    BenchmarkTrackingViewModel vm{fake};
    ASSERT_TRUE(vm.recentRankHistory()->runAxisShown());
    const QSignalSpy shown(vm.recentRankHistory(), &BenchmarkHistoryViewModel::runAxisShownChanged);

    fake->recentRankPresentation.xAxis = std::string(kCalendarXAxis);
    fake->publishPresentation();

    EXPECT_FALSE(vm.recentRankHistory()->runAxisShown());
    EXPECT_EQ(shown.count(), 1);
    EXPECT_EQ(vm.recentRankHistory()->xAxis().min(), static_cast<double>(expectedMidnightMs(ymd(2024, 3, 4))));
}
