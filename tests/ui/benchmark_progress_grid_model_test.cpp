// BenchmarkProgressGridModel is the Benchmarks tab's scenario progress grid that
// BenchmarkTrackingViewModel owns: one row per scenario in definition order, group cells spanning
// their rows, and the tier ladder the segmented bars and their header share. scenarioChunkFills is
// the bar's fraction mapping.

#include <gtest/gtest.h>

#include <QColor>
#include <QSignalSpy>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <memory>
#include <numeric>

#include "presentation/benchmark_tracking_vm.h"
#include "presentation/benchmark_progress_grid_model.h"
#include "presentation/benchmark_format.h"
#include "benchmarks/benchmark_evaluator.h"

#include "fake_benchmark_tracking_use_case.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;
using ksv::tests_ui::FakeBenchmarkTrackingUseCase;
using ksv::tests_ui::makeBreakdownSnapshot;
using ksv::tests_ui::vtTier;

namespace {
    std::vector<Tier> fiveTiers() {
        return {vtTier("iron", "Iron"), vtTier("bronze", "Bronze"), vtTier("silver", "Silver"),
                vtTier("gold", "Gold"), vtTier("platinum", "Platinum")};
    }

    // Iron 1000, Bronze 2000, Silver 3000, Gold 4000, Platinum 6000.
    std::vector<Threshold> fiveThresholds() {
        return {Threshold{TierId{"iron"}, 1000.0}, Threshold{TierId{"bronze"}, 2000.0},
                Threshold{TierId{"silver"}, 3000.0}, Threshold{TierId{"gold"}, 4000.0},
                Threshold{TierId{"platinum"}, 6000.0}};
    }

    void expectFills(const std::vector<double> &actual, const std::vector<double> &expected) {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index)
            EXPECT_NEAR(actual[index], expected[index], 1e-9) << "chunk " << index;
    }

    std::shared_ptr<FakeBenchmarkTrackingUseCase> gridFake() {
        auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
        fake->snap = makeBreakdownSnapshot();
        return fake;
    }

    QVariantMap rowFor(const BenchmarkProgressGridModel *grid, const QString &entryId) {
        for (const QVariant &row: grid->rows())
            if (row.toMap().value("entryId").toString() == entryId) return row.toMap();
        return {};
    }

    QVariantMap groupFor(const BenchmarkProgressGridModel *grid, const QString &id) {
        for (const QVariant &group: grid->groups())
            if (group.toMap().value("id").toString() == id) return group.toMap();
        return {};
    }
}

// --- the bar's fraction mapping ---------------------------------------------------------------

TEST(TierChunkFills, BelowTheFirstTierFillsTheFirstChunkFromZero) {
    expectFills(scenarioChunkFills(fiveTiers(), fiveThresholds(), 250.0), {0.25, 0, 0, 0, 0});
}

TEST(TierChunkFills, ExactlyOnAThresholdFillsThatTiersChunkAndNothingAfter) {
    expectFills(scenarioChunkFills(fiveTiers(), fiveThresholds(), 3000.0), {1, 1, 1, 0, 0});
    expectFills(scenarioChunkFills(fiveTiers(), fiveThresholds(), 1000.0), {1, 0, 0, 0, 0});
}

TEST(TierChunkFills, MidTierFillsTheNextChunkByTheInTierFraction) {
    // Halfway from Gold (4000) to Platinum (6000).
    expectFills(scenarioChunkFills(fiveTiers(), fiveThresholds(), 5000.0), {1, 1, 1, 1, 0.5});
    expectFills(scenarioChunkFills(fiveTiers(), fiveThresholds(), 2250.0), {1, 1, 0.25, 0, 0});
}

TEST(TierChunkFills, AboveTheTopTierFillsEveryChunkAndNoMore) {
    expectFills(scenarioChunkFills(fiveTiers(), fiveThresholds(), 9000.0), {1, 1, 1, 1, 1});
}

TEST(TierChunkFills, NoScoreFillsNothing) {
    expectFills(scenarioChunkFills(fiveTiers(), fiveThresholds(), std::nullopt), {0, 0, 0, 0, 0});
}

TEST(TierChunkFills, AChunkWithAMissingBoundStaysEmpty) {
    auto thresholds = fiveThresholds();
    thresholds.erase(thresholds.begin() + 2); // no Silver threshold
    // Silver's own chunk and Gold's (which starts at Silver) have no bounds; the rest fill.
    expectFills(scenarioChunkFills(fiveTiers(), thresholds, 4500.0), {1, 1, 0, 0, 0.25});
}

TEST(TierChunkFills, AChunkWhoseThresholdIsNotAboveThePreviousStaysEmpty) {
    auto thresholds = fiveThresholds();
    thresholds[1].score = 900.0; // Bronze below Iron
    expectFills(scenarioChunkFills(fiveTiers(), thresholds, 2500.0), {1, 0, 1600.0 / 2100.0, 0, 0});
}

TEST(TierChunkFills, FilledLengthMatchesTheDomainNormalizedRank) {
    const auto tiers = fiveTiers();
    const auto thresholds = fiveThresholds();
    for (const double score: {0.0, 1.0, 999.0, 1000.0, 1500.0, 2999.0, 3000.0, 4100.0, 5999.0, 6000.0, 8000.0}) {
        const auto fills = scenarioChunkFills(tiers, thresholds, score);
        EXPECT_NEAR(std::accumulate(fills.begin(), fills.end(), 0.0),
                    normalizedTierValue(tiers, thresholds, score), 1e-9) << score;
    }
}

// --- the recent-average marker's position --------------------------------------------------------

TEST(TierScalePosition, PlacesAScoreOnTheSameChunkScaleAsTheFills) {
    const auto tiers = fiveTiers();
    const auto thresholds = fiveThresholds();
    EXPECT_NEAR(*tierScalePosition(tiers, thresholds, 500.0), 0.5, 1e-9);   // below the first tier
    EXPECT_NEAR(*tierScalePosition(tiers, thresholds, 3000.0), 3.0, 1e-9);  // exactly on Silver
    EXPECT_NEAR(*tierScalePosition(tiers, thresholds, 5000.0), 4.5, 1e-9);  // mid Gold -> Platinum
    EXPECT_NEAR(*tierScalePosition(tiers, thresholds, 9000.0), 5.0, 1e-9);  // above the top tier
}

TEST(TierScalePosition, NoPositionWithoutAScoreOrAValidLadder) {
    EXPECT_FALSE(tierScalePosition(fiveTiers(), fiveThresholds(), std::nullopt).has_value());
    auto thresholds = fiveThresholds();
    thresholds.pop_back();
    EXPECT_FALSE(tierScalePosition(fiveTiers(), thresholds, 2000.0).has_value());
}

TEST(BenchmarkProgressGridModel, RecentAverageBelowThePersonalBestSitsLeftOfTheFillEnd) {
    auto fake = gridFake();
    // e1: PB 800, recent 780; Bronze 600, Silver 900, Gold 1200.
    fake->snap.selectedLoaded->benchmark.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t2"}, 900.0}, Threshold{TierId{"t3"}, 1200.0}};
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap e1 = rowFor(vm.progressGrid(), "e1");
    double fillEnd = 0.0;
    for (const QVariant &fill: e1.value("fills").toList()) fillEnd += fill.toDouble();
    const double recent = e1.value("recentPosition").toDouble();
    EXPECT_NEAR(recent, 1.0 + 180.0 / 300.0, 1e-9);
    EXPECT_LT(recent, fillEnd);
    EXPECT_EQ(e1.value("recentDescription").toString(),
              QString("Recent average %1 · Bronze").arg(formatBenchmarkNumber(780.0)));

    // No recent value (e2) or no valid ladder (every other row here): no marker.
    for (const char *id: {"e2", "u1", "e3"}) {
        EXPECT_LT(rowFor(vm.progressGrid(), id).value("recentPosition").toDouble(), 0.0) << id;
        EXPECT_TRUE(rowFor(vm.progressGrid(), id).value("recentDescription").toString().isEmpty()) << id;
    }
}

// --- the grid ------------------------------------------------------------------------------------

TEST(BenchmarkProgressGridModel, RowsFollowDefinitionOrderUnderSpanningGroups) {
    auto fake = gridFake();
    BenchmarkTrackingViewModel vm{fake};
    const BenchmarkProgressGridModel *grid = vm.progressGrid();
    ASSERT_NE(grid, nullptr);

    QStringList order;
    for (const QVariant &row: grid->rows()) order << row.toMap().value("entryId").toString();
    EXPECT_EQ(order, (QStringList{"u1", "u2", "e1", "e2", "e3"}));

    const QVariantMap uncategorized = groupFor(grid, QString());
    EXPECT_EQ(uncategorized.value("name").toString(), "Uncategorized");
    EXPECT_EQ(uncategorized.value("column").toString(), "category");
    EXPECT_EQ(uncategorized.value("start").toInt(), 0);
    EXPECT_EQ(uncategorized.value("span").toInt(), 2);
    EXPECT_TRUE(uncategorized.value("color").toString().isEmpty()) << "Uncategorized has no tint";

    const QVariantMap clicking = groupFor(grid, "c1");
    EXPECT_EQ(clicking.value("column").toString(), "category");
    EXPECT_EQ(clicking.value("start").toInt(), 2);
    EXPECT_EQ(clicking.value("span").toInt(), 2);
    EXPECT_EQ(QColor(clicking.value("color").toString()), QColor(255, 0, 0));

    const QVariantMap tracking = groupFor(grid, "c2");
    EXPECT_EQ(tracking.value("start").toInt(), 4);
    EXPECT_EQ(tracking.value("span").toInt(), 1);
    const QVariantMap smoothness = groupFor(grid, "sc1");
    EXPECT_EQ(smoothness.value("column").toString(), "subcategory");
    EXPECT_EQ(smoothness.value("name").toString(), "Smoothness");
    EXPECT_EQ(smoothness.value("start").toInt(), 4);
    EXPECT_EQ(smoothness.value("span").toInt(), 1);

    EXPECT_TRUE(grid->hasSubcategories());
    EXPECT_EQ(grid->groups().size(), 4);
}

TEST(BenchmarkProgressGridModel, GroupsWithoutScenariosAreLeftOut) {
    auto fake = gridFake();
    auto &definition = fake->snap.selectedLoaded->benchmark;
    definition.uncategorized.clear();
    definition.categories.push_back(Category{GroupId{"empty"}, "Empty", BenchmarkColor{1, 2, 3}, {}, {}});
    definition.categories.push_back(Category{GroupId{"hollow"}, "Hollow", BenchmarkColor{1, 2, 3}, {},
                                             {Subcategory{GroupId{"hollow-sub"}, "Hollow sub", {}, {}}}});
    definition.categories[0].subcategories.push_back(Subcategory{GroupId{"bare"}, "Bare", {}, {}});
    BenchmarkTrackingViewModel vm{fake};
    const BenchmarkProgressGridModel *grid = vm.progressGrid();

    QStringList ids;
    for (const QVariant &group: grid->groups()) ids << group.toMap().value("id").toString();
    ids.sort();
    EXPECT_EQ(ids, (QStringList{"c1", "c2", "sc1"}));
    EXPECT_EQ(grid->rows().size(), 3);
}

TEST(BenchmarkProgressGridModel, NoSubcategoryColumnWithoutSubcategories) {
    auto fake = gridFake();
    auto &definition = fake->snap.selectedLoaded->benchmark;
    definition.categories.pop_back(); // Tracking, the only category with a subcategory
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_FALSE(vm.progressGrid()->hasSubcategories());
}

TEST(BenchmarkProgressGridModel, TiersCarryReadableTextAndOpaqueBarColours) {
    auto fake = gridFake();
    auto &tiers = fake->snap.projection->tiers;
    tiers[0].color = BenchmarkColor{255, 200, 40};
    tiers[1].color = BenchmarkColor{20, 20, 60, 128}; // dark and translucent
    fake->snap.selectedLoaded->benchmark.tiers = tiers;
    BenchmarkTrackingViewModel vm{fake};

    const QVariantList gridTiers = vm.progressGrid()->tiers();
    ASSERT_EQ(gridTiers.size(), 3);
    const QVariantMap bronze = gridTiers.at(0).toMap();
    EXPECT_EQ(bronze.value("name").toString(), "Bronze");
    EXPECT_EQ(bronze.value("color").toString(), QColor(255, 200, 40).name());
    EXPECT_EQ(bronze.value("barColor").toString(), QColor(255, 200, 40).name());
    const QVariantMap silver = gridTiers.at(1).toMap();
    EXPECT_GE(contrastRatio(QColor(silver.value("color").toString()), darkPanelColor()), 4.5);
    EXPECT_EQ(silver.value("barColor").toString(), QColor(20, 20, 60).name());
}

TEST(BenchmarkProgressGridModel, ScenarioValuesShareOneNumberFormat) {
    auto fake = gridFake();
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    const QVariantMap e1 = rowFor(grid, "e1");
    EXPECT_EQ(e1.value("name").toString(), "1w6ts");
    EXPECT_EQ(e1.value("personalBestText").toString(), formatBenchmarkNumber(800.0));
    EXPECT_EQ(e1.value("recentText").toString(), formatBenchmarkNumber(780.0) + " (3)");
    EXPECT_EQ(e1.value("nextTierText").toString(), "Silver");
    EXPECT_EQ(e1.value("nextThresholdText").toString(), formatBenchmarkNumber(850.0));
    EXPECT_EQ(e1.value("attainedTierText").toString(), "Bronze");

    // Five recent runs: no sample count. No runs: a dash in both value columns.
    EXPECT_EQ(rowFor(grid, "e3").value("recentText").toString(), formatBenchmarkNumber(490.0));
    const QVariantMap e2 = rowFor(grid, "e2");
    EXPECT_EQ(e2.value("personalBestText").toString(), "—");
    EXPECT_EQ(e2.value("recentText").toString(), "—");
    EXPECT_EQ(e2.value("attainedTierText").toString(), "Unranked");
}

TEST(BenchmarkProgressGridModel, BarFillsAndDescriptionPlaceThePersonalBestOnTheLadder) {
    auto fake = gridFake();
    // e1 (PB 800): Bronze 600, Silver 900, Gold 1200 -> Bronze, two thirds of the way to Silver.
    fake->snap.selectedLoaded->benchmark.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t2"}, 900.0}, Threshold{TierId{"t3"}, 1200.0}};
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap e1 = rowFor(vm.progressGrid(), "e1");
    const QVariantList fills = e1.value("fills").toList();
    ASSERT_EQ(fills.size(), 3);
    EXPECT_DOUBLE_EQ(fills.at(0).toDouble(), 1.0);
    EXPECT_NEAR(fills.at(1).toDouble(), 2.0 / 3.0, 1e-9);
    EXPECT_DOUBLE_EQ(fills.at(2).toDouble(), 0.0);
    EXPECT_EQ(e1.value("barDescription").toString(),
              QString("Bronze · 66% of the way to Silver (%1 / %2)")
                  .arg(formatBenchmarkNumber(800.0), formatBenchmarkNumber(900.0)));
}

TEST(BenchmarkProgressGridModel, BarDescriptionNamesUnrankedUnplayedAndTheTopTier) {
    auto fake = gridFake();
    auto &definition = fake->snap.selectedLoaded->benchmark;
    // e1 (PB 800) below Bronze 1000; e3 (PB 500) above every threshold.
    definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 1000.0}, Threshold{TierId{"t2"}, 2000.0}, Threshold{TierId{"t3"}, 3000.0}};
    definition.categories[1].subcategories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 100.0}, Threshold{TierId{"t2"}, 200.0}, Threshold{TierId{"t3"}, 300.0}};
    fake->snap.projection->scenarios[4].nextThreshold.reset();
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    EXPECT_TRUE(rowFor(grid, "e1").value("barDescription").toString()
                    .startsWith("Unranked · 80% of the way to Bronze"));
    EXPECT_TRUE(rowFor(grid, "e2").value("barDescription").toString().startsWith("Unplayed"));
    const QVariantMap e3 = rowFor(grid, "e3");
    EXPECT_TRUE(e3.value("barDescription").toString().startsWith("Gold · top tier reached"));
    EXPECT_EQ(e3.value("nextThresholdText").toString(), "Highest tier attained");
}

TEST(BenchmarkProgressGridModel, OnlyAbnormalMatchStatesAreReportedInTheUsersTerms) {
    auto fake = gridFake();
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    const QVariantMap e1 = rowFor(grid, "e1");
    EXPECT_TRUE(e1.value("matchStateText").toString().isEmpty());
    EXPECT_TRUE(e1.value("matchStateHint").toString().isEmpty());
    EXPECT_FALSE(e1.value("barDescription").toString().contains('\n'));

    EXPECT_EQ(rowFor(grid, "e2").value("matchStateText").toString(), "No runs of linked scenario");
    EXPECT_EQ(rowFor(grid, "u1").value("matchStateText").toString(), "Not found in your runs");
    EXPECT_EQ(rowFor(grid, "e3").value("matchStateText").toString(), "Several scenarios match");
    EXPECT_TRUE(rowFor(grid, "u2").value("matchStateText").toString().startsWith("Linking to your runs"));
    for (const char *id: {"e2", "u1", "e3", "u2"}) {
        const QVariantMap row = rowFor(grid, id);
        EXPECT_FALSE(row.value("matchStateHint").toString().isEmpty()) << id;
        EXPECT_TRUE(row.value("barDescription").toString().contains(row.value("matchStateText").toString())) << id;
    }
    for (const QVariant &row: grid->rows()) {
        const QString text = row.toMap().value("matchStateText").toString();
        EXPECT_FALSE(text.contains("Resolved") || text.contains("Unresolved") || text.contains("Mapped"))
            << text.toStdString();
    }
}

TEST(BenchmarkProgressGridModel, AnEntryMissingFromTheProjectionIsReportedNotFound) {
    auto fake = gridFake();
    auto &scenarios = fake->snap.projection->scenarios;
    scenarios.erase(std::remove_if(scenarios.begin(), scenarios.end(),
                                   [](const ScenarioProjection &s) { return s.entryId.value == "e1"; }),
                    scenarios.end());
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap e1 = rowFor(vm.progressGrid(), "e1");
    EXPECT_EQ(e1.value("name").toString(), "1w6ts");
    EXPECT_EQ(e1.value("matchStateText").toString(), "Not found in your runs");
    EXPECT_EQ(e1.value("personalBestText").toString(), "—");
}

TEST(MissingThresholdChunks, FlagsChunksWithAnUnusableBound) {
    EXPECT_EQ(missingThresholdChunks(fiveTiers(), fiveThresholds()),
              (std::vector<bool>{false, false, false, false, false}));
    auto thresholds = fiveThresholds();
    thresholds.erase(thresholds.begin() + 2);     // no Silver: Silver's and Gold's chunks
    thresholds.back().score = 3500.0;             // Platinum 3500 below Gold 4000
    EXPECT_EQ(missingThresholdChunks(fiveTiers(), thresholds), (std::vector<bool>{false, false, true, true, true}));
}

TEST(BenchmarkProgressGridModel, EdgeStatesUnplayedUnmatchedAndAboveTheTopTier) {
    auto fake = gridFake();
    auto &definition = fake->snap.selectedLoaded->benchmark;
    // e1 (PB 800, Resolved) above Gold 700; e3 is Ambiguous; e2 has no runs of its linked scenario.
    definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 300.0}, Threshold{TierId{"t2"}, 500.0}, Threshold{TierId{"t3"}, 700.0}};
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    const QVariantMap e1 = rowFor(grid, "e1");
    EXPECT_EQ(e1.value("edgeState").toString(), "aboveTop");
    EXPECT_EQ(e1.value("aboveTopText").toString(),
              QString("Above Gold: %1 / %2").arg(formatBenchmarkNumber(800.0), formatBenchmarkNumber(700.0)));

    const QVariantMap e2 = rowFor(grid, "e2");
    EXPECT_EQ(e2.value("edgeState").toString(), "unplayed");
    EXPECT_EQ(e2.value("statusText").toString(), "Unplayed · no runs of the linked scenario");

    for (const char *id: {"u1", "e3", "u2"}) {
        const QVariantMap row = rowFor(grid, id);
        EXPECT_EQ(row.value("edgeState").toString(), "unmatched") << id;
        EXPECT_EQ(row.value("statusText").toString(), row.value("matchStateText").toString()) << id;
        for (const QVariant &fill: row.value("fills").toList()) EXPECT_EQ(fill.toDouble(), 0.0) << id;
    }

    // Exactly on the top threshold is reached, not exceeded.
    fake->snap.projection->scenarios[2].personalBest = 700.0;
    fake->publish();
    EXPECT_TRUE(rowFor(grid, "e1").value("edgeState").toString().isEmpty());
}

TEST(BenchmarkProgressGridModel, MissingThresholdChunksTravelWithTheRow) {
    auto fake = gridFake();
    fake->snap.selectedLoaded->benchmark.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t3"}, 1200.0}};
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap e1 = rowFor(vm.progressGrid(), "e1");
    QList<bool> missing;
    for (const QVariant &flag: e1.value("missingChunks").toList()) missing << flag.toBool();
    EXPECT_EQ(missing, (QList<bool>{false, true, true}));
    const QVariantList fills = e1.value("fills").toList();
    EXPECT_EQ(fills.at(1).toDouble(), 0.0);
    EXPECT_EQ(fills.at(2).toDouble(), 0.0) << "no partial fill in a chunk without a usable bound";
}

TEST(BenchmarkProgressGridModel, BlockingRowsNameTheNextTierInItsColour) {
    auto fake = gridFake();
    fake->snap.projection->tiers[1].color = BenchmarkColor{192, 192, 200}; // Silver, the next tier
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    const QVariantMap e2 = rowFor(grid, "e2");
    EXPECT_TRUE(e2.value("blocker").toBool());
    EXPECT_EQ(e2.value("blockerText").toString(), QString("Blocks Silver (needs %1)").arg(formatBenchmarkNumber(850.0)));
    EXPECT_EQ(e2.value("blockerColor").toString(), QColor(192, 192, 200).name());
    EXPECT_TRUE(rowFor(grid, "e3").value("blocker").toBool());
    for (const char *id: {"u1", "u2", "e1"}) {
        const QVariantMap row = rowFor(grid, id);
        EXPECT_FALSE(row.value("blocker").toBool()) << id;
        EXPECT_TRUE(row.value("blockerText").toString().isEmpty()) << id;
    }
}

TEST(BenchmarkProgressGridModel, EmptiesOutsideTheReadyStatesAndNotifiesOncePerRevision) {
    auto fake = gridFake();
    BenchmarkTrackingViewModel vm{fake};
    BenchmarkProgressGridModel *grid = vm.progressGrid();
    ASSERT_FALSE(grid->rows().isEmpty());

    const QSignalSpy spy(grid, &BenchmarkProgressGridModel::changed);
    fake->snap.availability = BenchmarkTrackingState::Unavailable;
    fake->snap.selectedLoaded.reset();
    fake->snap.projection.reset();
    fake->publish();

    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(grid->rows().isEmpty());
    EXPECT_TRUE(grid->groups().isEmpty());
    EXPECT_TRUE(grid->tiers().isEmpty());
    EXPECT_FALSE(grid->hasSubcategories());
}
