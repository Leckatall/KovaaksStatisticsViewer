// BenchmarkProgressGridModel is the Benchmarks tab's scenario progress grid that
// BenchmarkTrackingViewModel owns: one row per scenario in definition order, group cells spanning
// their rows, and the tier ladder the segmented bars and their header share. scenarioChunkFills is
// the bar's fraction mapping.

#include <gtest/gtest.h>

#include <QColor>
#include <QDateTime>
#include <QLocale>
#include <QSignalSpy>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <limits>
#include <memory>
#include <numeric>
#include <set>

#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"
#include "presentation/benchmarks/tracking/benchmark_progress_grid_model.h"
#include "presentation/benchmarks/benchmark_format.h"

#include "fake_benchmark_tracking_use_case.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;
using namespace ksv::tests_ui;
using namespace ksv::tests_support;

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

    struct ScoredScenario {
        Benchmark benchmark;

        explicit ScoredScenario(std::vector<Threshold> thresholds = fiveThresholds(),
                                std::vector<Tier> tiers = fiveTiers())
            : benchmark(evaluatedBenchmark(
                  {BenchmarkId{"grid"}, "Grid", std::move(tiers),
                   {{BenchmarkScenarioId{"entry"}, "Scenario", {"hash"}, std::move(thresholds)}}, {}}, {})) {}

        const ScenarioBenchmarkEvaluation &scenario() const { return scenarioNamed(benchmark, "entry"); }
        std::vector<double> fills(std::optional<double> score) const {
            return scenarioChunkFills(benchmark.tiers(), scenario(), score);
        }
        std::vector<bool> missing() const { return missingThresholdChunks(benchmark.tiers(), scenario()); }
        std::optional<double> position(std::optional<double> score) const {
            return tierScalePosition(scenario(), score);
        }
    };

    BenchmarkFixture breakdownWithTopTier() {
        auto fixture = makeBreakdownFixture();
        fixture.definition.categories[0].scenarios[0].thresholds = standardThresholds(1000, 2000, 3000);
        fixture.definition.categories[1].subcategories[0].scenarios[0].thresholds = standardThresholds(100, 200, 300);
        fixture.resolutions.back() = resolvedEntry("e3");
        fixture.runs.insert(fixture.runs.begin(), runOn(ymd(2024, 3, 4), "h3", 500, 60));
        return fixture;
    }

    BenchmarkFixture breakdownWithRankedGroups() {
        auto fixture = makeBreakdownFixture();
        fixture.definition.tiers[0].color = {205, 127, 50};
        fixture.definition.tiers[1].color = {20, 20, 60};
        fixture.resolutions.back() = resolvedEntry("e3");
        fixture.runs.insert(fixture.runs.begin(), runOn(ymd(2024, 3, 4), "h3", 700, 60));
        return fixture;
    }

    BenchmarkFixture breakdownWithManyPlays() {
        auto fixture = makeBreakdownFixture();
        const QDateTime set(QDate(2026, 8, 14), QTime(21, 30));
        fixture.runs = {benchmarkFact("h1", set.toMSecsSinceEpoch(), 800, 60)};
        for (int i = 1; i < 1234; ++i)
            fixture.runs.push_back(benchmarkFact("h1", set.toMSecsSinceEpoch() + 60000LL * i, 100, 60));
        return fixture;
    }

    std::shared_ptr<FakeBenchmarkTrackingUseCase> gridFake(BenchmarkFixture fixture = makeBreakdownFixture()) {
        auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
        fake->show(std::move(fixture));
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

// Each chunk is the progress toward the tier it is named after: Iron's from 0 to Iron's
// threshold, each later one from the previous threshold to its own.

TEST(TierChunkFills, BelowTheFirstTierFillsTheIronChunkFromZero) {
    expectFills(ScoredScenario{}.fills(250.0), {0.25, 0, 0, 0, 0});
}

TEST(TierChunkFills, ExactlyOnAThresholdCompletesThatTiersChunkAndNothingAfter) {
    expectFills(ScoredScenario{}.fills(3000.0), {1, 1, 1, 0, 0});
    expectFills(ScoredScenario{}.fills(1000.0), {1, 0, 0, 0, 0});
}

TEST(TierChunkFills, MidTierFillsTheNextTiersChunkByTheInTierFraction) {
    // Halfway from Gold (4000) to Platinum (6000): Platinum's chunk half full.
    expectFills(ScoredScenario{}.fills(5000.0), {1, 1, 1, 1, 0.5});
    expectFills(ScoredScenario{}.fills(2250.0), {1, 1, 0.25, 0, 0});
}

TEST(TierChunkFills, ReachingTheTopTierFillsTheWholeBar) {
    expectFills(ScoredScenario{}.fills(5999.0), {1, 1, 1, 1, 1999.0 / 2000.0});
    expectFills(ScoredScenario{}.fills(6000.0), {1, 1, 1, 1, 1});
    expectFills(ScoredScenario{}.fills(9000.0), {1, 1, 1, 1, 1});
}

TEST(TierChunkFills, NoScoreFillsNothing) {
    expectFills(ScoredScenario{}.fills(std::nullopt), {0, 0, 0, 0, 0});
}

TEST(TierChunkFills, AChunkWithAMissingBoundStaysEmpty) {
    auto thresholds = fiveThresholds();
    thresholds.erase(thresholds.begin() + 2); // no Silver threshold
    // Silver's own chunk and Gold's (which starts at Silver) have no bounds; the rest fill.
    expectFills(ScoredScenario{thresholds}.fills(4500.0), {1, 1, 0, 0, 0.25});
}

// Bronze below Iron: either may be the wrong one, so every chunk either bounds (Iron, Bronze and
// Silver, whose lower bound is Bronze) stays empty rather than showing a fill from a doubtful bound.
TEST(TierChunkFills, ChunksBoundedByAnOutOfOrderThresholdStayEmpty) {
    auto thresholds = fiveThresholds();
    thresholds[1].score = 900.0; // Bronze below Iron
    expectFills(ScoredScenario{thresholds}.fills(2500.0), {0, 0, 0, 0, 0});
    expectFills(ScoredScenario{thresholds}.fills(5000.0), {0, 0, 0, 1, 0.5});
}

// The filled length is the domain's normalized rank (6.37 = six full chunks), and the marker
// scale is the same.
TEST(TierChunkFills, FilledLengthMatchesTheDomainNormalizedRankAndTheMarkerScale) {
    const ScoredScenario scored;
    for (const double score: {0.0, 1.0, 999.0, 1000.0, 1500.0, 2999.0, 3000.0, 4100.0, 5999.0, 6000.0, 8000.0}) {
        const auto fills = scored.fills(score);
        const double length = std::accumulate(fills.begin(), fills.end(), 0.0);
        EXPECT_NEAR(length, scored.scenario().normalized(score), 1e-9) << score;
        const auto position = scored.position(score);
        ASSERT_TRUE(position);
        EXPECT_NEAR(length, *position, 1e-9) << score;
    }
}

// --- the recent-average marker's position --------------------------------------------------------

TEST(TierScalePosition, PlacesAScoreOnTheSameChunkScaleAsTheFills) {
    const ScoredScenario scored;
    EXPECT_NEAR(scored.position(500.0).value_or(-1.0), 0.5, 1e-9);   // below the first tier: inside Iron's chunk
    EXPECT_NEAR(scored.position(3000.0).value_or(-1.0), 3.0, 1e-9);  // exactly on Silver: its chunk's end
    EXPECT_NEAR(scored.position(5000.0).value_or(-1.0), 4.5, 1e-9);  // mid Gold -> Platinum
    EXPECT_NEAR(scored.position(6000.0).value_or(-1.0), 5.0, 1e-9);  // the top tier: bar end
    EXPECT_NEAR(scored.position(9000.0).value_or(-1.0), 5.0, 1e-9);  // above the top tier
}

TEST(TierScalePosition, NoPositionWithoutAScoreOrAValidLadder) {
    EXPECT_FALSE(ScoredScenario{}.position(std::nullopt).has_value());
    auto thresholds = fiveThresholds();
    thresholds.pop_back();
    EXPECT_FALSE(ScoredScenario{thresholds}.position(2000.0).has_value());
}

TEST(BenchmarkProgressGridModel, RecentAverageBelowThePersonalBestSitsLeftOfTheFillEnd) {
    auto fake = gridFake();
    // e1: PB 800, recent 780; Bronze 600, Silver 900, Gold 1200.
    fake->fixture.definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t2"}, 900.0}, Threshold{TierId{"t3"}, 1200.0}};
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap e1 = rowFor(vm.progressGrid(), "e1");
    double fillEnd = 0.0;
    for (const QVariant &fill: e1.value("fills").toList()) fillEnd += fill.toDouble();
    const double recent = e1.value("recentPosition").toDouble();
    EXPECT_NEAR(recent, 1.0 + 180.0 / 300.0, 1e-9);
    EXPECT_LT(recent, fillEnd);
    EXPECT_EQ(e1.value("recentDescription").toString(),
              QString("Recent average %1 · Bronze").arg(formatBenchmarkNumber(780.0)));

    // Scenarios without runs have no recent marker.
    for (const char *id: {"e2", "u1", "e3"}) {
        EXPECT_LT(rowFor(vm.progressGrid(), id).value("recentPosition").toDouble(), 0.0) << id;
        EXPECT_TRUE(rowFor(vm.progressGrid(), id).value("recentDescription").toString().isEmpty()) << id;
    }
}

TEST(BenchmarkProgressGridContent, MapsAreDerivedFromTheTypedForm) {
    auto fake = gridFake();
    auto &definition = fake->fixture.definition;
    definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 300.0}, Threshold{TierId{"t2"}, 500.0}, Threshold{TierId{"t3"}, 700.0}};
    definition.categories[1].subcategories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t3"}, 1200.0}};
    fake->fixture.completeness = CompletenessResult{Completeness::Trackable, {}};
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};
    const BenchmarkProgressGridModel *grid = vm.progressGrid();
    const ProgressGridContent &content = grid->content();

    ASSERT_EQ(static_cast<qsizetype>(content.tiers.size()), grid->tiers().size());
    ASSERT_EQ(static_cast<qsizetype>(content.rows.size()), grid->rows().size());
    ASSERT_EQ(static_cast<qsizetype>(content.groups.size()), grid->groups().size());
    EXPECT_EQ(content.hasSubcategories, grid->hasSubcategories());

    const QVariantMap headers = grid->headers();
    EXPECT_EQ(headers.value("category").toString(), content.headings.category);
    EXPECT_EQ(headers.value("subcategory").toString(), content.headings.subcategory);
    EXPECT_EQ(headers.value("scenario").toString(), content.headings.scenario);
    EXPECT_EQ(headers.value("progress").toString(), content.headings.progress);
    EXPECT_EQ(headers.value("pbDate").toString(), content.headings.pbDate);
    EXPECT_EQ(headers.value("plays").toString(), content.headings.plays);
    EXPECT_EQ(headers.value("next").toString(), content.headings.next);

    for (qsizetype index = 0; index < grid->tiers().size(); ++index) {
        const QVariantMap map = grid->tiers().at(index).toMap();
        const ProgressGridTier &tier = content.tiers[static_cast<std::size_t>(index)];
        EXPECT_EQ(map.value("id").toString(), tier.id);
        EXPECT_EQ(map.value("name").toString(), tier.name);
        EXPECT_EQ(map.value("color").toString(), tier.color);
        EXPECT_EQ(map.value("barColor").toString(), tier.barColor);
    }

    std::set<ProgressGridEdgeState> edgeStates;
    bool sawBlocker = false;
    bool sawRecent = false;
    bool sawMissingChunk = false;
    const auto walkRows = [&] {
    for (qsizetype index = 0; index < grid->rows().size(); ++index) {
        const QVariantMap map = grid->rows().at(index).toMap();
        const ProgressGridScenarioRow &row = content.rows[static_cast<std::size_t>(index)];
        edgeStates.insert(row.edgeState);
        sawBlocker |= row.blocker;
        sawRecent |= row.recentPosition.has_value();
        sawMissingChunk |= std::ranges::any_of(row.missingChunks, [](const bool missing) { return missing; });

        EXPECT_EQ(map.value("entryId").toString(), row.entryId);
        EXPECT_EQ(map.value("name").toString(), row.name);
        EXPECT_EQ(map.value("personalBestText").toString(), row.personalBestText);
        EXPECT_EQ(map.value("personalBestDateText").toString(), row.personalBestDateText);
        EXPECT_EQ(map.value("playCountText").toString(), row.playCountText);
        EXPECT_EQ(map.value("attainedTierText").toString(), row.attainedTierText);
        EXPECT_EQ(map.value("attainedTierColor").toString(), row.attainedTierColor);
        EXPECT_EQ(map.value("recentPosition").toDouble(), row.recentPosition.value_or(-1.0));
        EXPECT_EQ(map.value("recentDescription").toString(), row.recentDescription);
        EXPECT_EQ(map.value("nextTierText").toString(), row.nextTierText);
        EXPECT_EQ(map.value("nextTierColor").toString(), row.nextTierColor);
        EXPECT_EQ(map.value("nextThresholdText").toString(), row.nextThresholdText);
        EXPECT_EQ(map.value("nextText").toString(), row.nextText);
        EXPECT_EQ(map.value("matchStateText").toString(), row.matchStateText);
        EXPECT_EQ(map.value("matchStateHint").toString(), row.matchStateHint);
        EXPECT_EQ(map.value("barDescription").toString(), row.barDescription);
        EXPECT_EQ(map.value("statusText").toString(), row.statusText);
        EXPECT_EQ(map.value("aboveTopText").toString(), row.aboveTopText);
        EXPECT_EQ(map.value("blocker").toBool(), row.blocker);
        EXPECT_EQ(map.value("blockerColor").toString(), row.blockerColor);
        EXPECT_EQ(map.value("blockerText").toString(), row.blockerText);

        const QString edgeKey = map.value("edgeState").toString();
        switch (row.edgeState) {
            case ProgressGridEdgeState::None: EXPECT_EQ(edgeKey, QString()); break;
            case ProgressGridEdgeState::Unplayed: EXPECT_EQ(edgeKey, "unplayed"); break;
            case ProgressGridEdgeState::Unmatched: EXPECT_EQ(edgeKey, "unmatched"); break;
            case ProgressGridEdgeState::AboveTop: EXPECT_EQ(edgeKey, "aboveTop"); break;
        }

        const QVariantList fills = map.value("fills").toList();
        ASSERT_EQ(static_cast<std::size_t>(fills.size()), row.fills.size());
        for (qsizetype fill = 0; fill < fills.size(); ++fill)
            EXPECT_EQ(fills.at(fill).toDouble(), row.fills[static_cast<std::size_t>(fill)]);
        const QVariantList missing = map.value("missingChunks").toList();
        ASSERT_EQ(static_cast<std::size_t>(missing.size()), row.missingChunks.size());
        for (qsizetype chunk = 0; chunk < missing.size(); ++chunk)
            EXPECT_EQ(missing.at(chunk).toBool(), static_cast<bool>(row.missingChunks[static_cast<std::size_t>(chunk)]));
        const QVariantList thresholds = map.value("thresholdTexts").toList();
        ASSERT_EQ(static_cast<std::size_t>(thresholds.size()), row.thresholdTexts.size());
        for (qsizetype text = 0; text < thresholds.size(); ++text)
            EXPECT_EQ(thresholds.at(text).toString(), row.thresholdTexts[static_cast<std::size_t>(text)]);
    }
    };
    walkRows();
    // e1 back within its ladder gives a row with no edge state.
    definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t2"}, 900.0}, Threshold{TierId{"t3"}, 1200.0}};
    fake->reevaluate();
    fake->publish();
    walkRows();

    std::set<ProgressGridGroupColumn> columns;
    for (qsizetype index = 0; index < grid->groups().size(); ++index) {
        const QVariantMap map = grid->groups().at(index).toMap();
        const ProgressGridGroup &group = content.groups[static_cast<std::size_t>(index)];
        columns.insert(group.column);
        EXPECT_EQ(map.value("column").toString(),
                  group.column == ProgressGridGroupColumn::Category ? QString("category") : QString("subcategory"));
        EXPECT_EQ(map.value("id").toString(), group.id);
        EXPECT_EQ(map.value("name").toString(), group.name);
        EXPECT_EQ(map.value("color").toString(), group.color);
        EXPECT_EQ(map.value("start").toInt(), group.start);
        EXPECT_EQ(map.value("span").toInt(), group.span);
        EXPECT_EQ(map.value("spansSubcategory").toBool(), group.spansSubcategory);
        EXPECT_EQ(map.value("rankText").toString(), group.rankText);
        EXPECT_EQ(map.value("rankColor").toString(), group.rankColor);
        EXPECT_EQ(map.value("rankBarColor").toString(), group.rankBarColor);
    }

    EXPECT_TRUE(edgeStates.contains(ProgressGridEdgeState::None));
    EXPECT_TRUE(edgeStates.contains(ProgressGridEdgeState::Unplayed));
    EXPECT_TRUE(edgeStates.contains(ProgressGridEdgeState::Unmatched));
    EXPECT_TRUE(edgeStates.contains(ProgressGridEdgeState::AboveTop));
    EXPECT_TRUE(sawBlocker);
    EXPECT_TRUE(sawRecent);
    EXPECT_TRUE(sawMissingChunk);
    EXPECT_EQ(columns.size(), 2u) << "the fixture must have a subcategory group";
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

TEST(BenchmarkProgressGridModel, GroupsCarryTheirRankInTheTierColour) {
    auto fake = gridFake(breakdownWithRankedGroups());
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    const QVariantMap clicking = groupFor(grid, "c1");
    EXPECT_EQ(clicking.value("rankText").toString(), "Bronze");
    EXPECT_EQ(clicking.value("rankBarColor").toString(), QColor(205, 127, 50).name());
    const QVariantMap smoothness = groupFor(grid, "sc1");
    EXPECT_EQ(smoothness.value("rankText").toString(), "Silver");
    EXPECT_GE(contrastRatio(QColor(smoothness.value("rankColor").toString()), darkPanelColor()), 4.5);
    EXPECT_NE(clicking.value("rankBarColor"), smoothness.value("rankBarColor"));

    EXPECT_EQ(groupFor(grid, "c2").value("rankText").toString(), "Silver");
    fake->fixture.runs.erase(fake->fixture.runs.begin());
    fake->reevaluate();
    fake->publish();
    for (const char *id: {"c2", "sc1"}) {
        EXPECT_EQ(groupFor(grid, id).value("rankText").toString(), "Unranked");
        EXPECT_TRUE(groupFor(grid, id).value("rankColor").toString().isEmpty());
    }
    EXPECT_TRUE(groupFor(grid, QString()).value("rankText").toString().isEmpty())
        << "Uncategorized is not ranked as a group";

    // An incomplete benchmark ranks nothing, so no group shows a chip.
    fake->fixture.completeness = CompletenessResult{Completeness::Incomplete, {}};
    fake->reevaluate();
    fake->publish();
    for (const QVariant &group: grid->groups())
        EXPECT_TRUE(group.toMap().value("rankText").toString().isEmpty());
}

TEST(BenchmarkProgressGridModel, GroupsWithoutScenariosAreLeftOut) {
    auto fake = gridFake();
    auto &definition = fake->fixture.definition;
    definition.uncategorized.clear();
    definition.categories.push_back(Category{GroupId{"empty"}, "Empty", BenchmarkColor{1, 2, 3}, {}, {}});
    definition.categories.push_back(Category{GroupId{"hollow"}, "Hollow", BenchmarkColor{1, 2, 3}, {},
                                             {Subcategory{GroupId{"hollow-sub"}, "Hollow sub", {}, {}}}});
    definition.categories[0].subcategories.push_back(Subcategory{GroupId{"bare"}, "Bare", {}, {}});
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};
    const BenchmarkProgressGridModel *grid = vm.progressGrid();

    QStringList ids;
    for (const QVariant &group: grid->groups()) ids << group.toMap().value("id").toString();
    ids.sort();
    EXPECT_EQ(ids, (QStringList{"c1", "c2", "sc1"}));
    EXPECT_EQ(grid->rows().size(), 3);
}

TEST(BenchmarkProgressGridModel, CategoriesWithoutASubcategoryRowSpanTheSubcategoryColumn) {
    auto fake = gridFake();
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    EXPECT_TRUE(groupFor(grid, QString()).value("spansSubcategory").toBool());
    EXPECT_TRUE(groupFor(grid, "c1").value("spansSubcategory").toBool());
    EXPECT_FALSE(groupFor(grid, "c2").value("spansSubcategory").toBool());
    EXPECT_TRUE(groupFor(grid, "sc1").contains("spansSubcategory"));
    EXPECT_FALSE(groupFor(grid, "sc1").value("spansSubcategory").toBool());

    // A mixed category: its direct scenarios come first, so the subcategory starts mid-span.
    auto &definition = fake->fixture.definition;
    definition.categories[0].subcategories.push_back(
        Subcategory{GroupId{"sc2"}, "Mixed", {}, {definition.uncategorized.front()}});
    definition.uncategorized.erase(definition.uncategorized.begin());
    fake->reevaluate();
    fake->publish();
    EXPECT_FALSE(groupFor(grid, "c1").value("spansSubcategory").toBool());
    EXPECT_TRUE(groupFor(grid, QString()).value("spansSubcategory").toBool());
}

TEST(BenchmarkProgressGridModel, NoSubcategoryColumnWithoutSubcategories) {
    auto fake = gridFake();
    auto &definition = fake->fixture.definition;
    definition.categories.pop_back(); // Tracking, the only category with a subcategory
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_FALSE(vm.progressGrid()->hasSubcategories());
}

TEST(BenchmarkProgressGridModel, TiersCarryReadableTextAndOpaqueBarColours) {
    auto fake = gridFake();
    auto &tiers = fake->fixture.definition.tiers;
    tiers[0].color = BenchmarkColor{255, 200, 40};
    tiers[1].color = BenchmarkColor{20, 20, 60, 128}; // dark and translucent
    fake->reevaluate();
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
    EXPECT_EQ(e1.value("nextTierText").toString(), "Silver");
    EXPECT_EQ(e1.value("nextThresholdText").toString(), formatBenchmarkNumber(850.0));
    EXPECT_EQ(e1.value("attainedTierText").toString(), "Bronze");

    // No runs: a dash in every value column.
    const QVariantMap e2 = rowFor(grid, "e2");
    EXPECT_EQ(e2.value("personalBestText").toString(), "—");
    EXPECT_EQ(e2.value("personalBestDateText").toString(), "—");
    EXPECT_EQ(e2.value("playCountText").toString(), "—");
    EXPECT_EQ(e2.value("attainedTierText").toString(), "Unranked");
}

TEST(BenchmarkProgressGridModel, RowsCarryThePersonalBestDateAndPlayCount) {
    auto fake = gridFake(breakdownWithManyPlays());
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap row = rowFor(vm.progressGrid(), "e1");
    EXPECT_EQ(row.value("personalBestDateText").toString(), "2026-08-14");
    EXPECT_EQ(row.value("playCountText").toString(), QLocale().toString(1234));
}

TEST(BenchmarkProgressGridContent, PresenceFlagsFollowTheEvaluation) {
    auto fake = gridFake(breakdownWithManyPlays());
    BenchmarkTrackingViewModel vm{fake};

    const ProgressGridContent content = vm.progressGrid()->content();
    const auto find = [&](const QString &entryId) -> const ProgressGridScenarioRow & {
        const auto it = std::ranges::find(content.rows, entryId, &ProgressGridScenarioRow::entryId);
        EXPECT_NE(it, content.rows.end());
        return *it;
    };

    const ProgressGridScenarioRow &playedRow = find("e1");
    EXPECT_TRUE(playedRow.hasPersonalBest);
    EXPECT_TRUE(playedRow.hasPersonalBestDate);
    EXPECT_TRUE(playedRow.hasPlayCount);

    const ProgressGridScenarioRow &unplayedRow = find("e2");
    EXPECT_FALSE(unplayedRow.hasPersonalBest);
    EXPECT_FALSE(unplayedRow.hasPersonalBestDate);
    EXPECT_FALSE(unplayedRow.hasPlayCount);

    EXPECT_TRUE(rowFor(vm.progressGrid(), "e1").value("hasPersonalBest").toBool());
}

TEST(BenchmarkProgressGridModel, BarFillsAndDescriptionPlaceThePersonalBestOnTheLadder) {
    auto fake = gridFake();
    // e1 (PB 800): Bronze 600, Silver 900, Gold 1200 -> Bronze, two thirds of the way to Silver.
    fake->fixture.definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t2"}, 900.0}, Threshold{TierId{"t3"}, 1200.0}};
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap e1 = rowFor(vm.progressGrid(), "e1");
    const QVariantList fills = e1.value("fills").toList();
    ASSERT_EQ(fills.size(), 3);
    EXPECT_DOUBLE_EQ(fills.at(0).toDouble(), 1.0) << "Bronze is reached";
    EXPECT_NEAR(fills.at(1).toDouble(), 2.0 / 3.0, 1e-9) << "two thirds of the way to Silver";
    EXPECT_DOUBLE_EQ(fills.at(2).toDouble(), 0.0);
    EXPECT_EQ(e1.value("barDescription").toString(),
              QString("Bronze · 66% of the way to Silver (%1 / %2)")
                  .arg(formatBenchmarkNumber(800.0), formatBenchmarkNumber(900.0)));
}

TEST(BenchmarkProgressGridModel, BarDescriptionNamesUnrankedUnplayedAndTheTopTier) {
    auto fake = gridFake(breakdownWithTopTier());
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

TEST(BenchmarkProgressGridModel, AnEntryWithoutAResolutionIsReportedNotFound) {
    auto fake = gridFake();
    std::erase_if(fake->fixture.resolutions, [](const ScenarioResolution &resolution) {
        return resolution.entryId.value == "e1";
    });
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap e1 = rowFor(vm.progressGrid(), "e1");
    EXPECT_EQ(e1.value("name").toString(), "1w6ts");
    EXPECT_EQ(e1.value("matchStateText").toString(), "Not found in your runs");
    EXPECT_EQ(e1.value("personalBestText").toString(), "—");
}

TEST(MissingThresholdChunks, FlagsChunksWithAnUnusableBound) {
    EXPECT_EQ(ScoredScenario{}.missing(),
              (std::vector<bool>{false, false, false, false, false}));
    auto thresholds = fiveThresholds();
    thresholds.erase(thresholds.begin() + 2);     // no Silver: Silver's and Gold's chunks
    thresholds.back().score = 3500.0;             // Platinum 3500 below Gold 4000: Gold's and Platinum's
    EXPECT_EQ(ScoredScenario{thresholds}.missing(), (std::vector<bool>{false, false, true, true, true}));
}

// Iron 1800, Bronze 1200, Silver 2400: either Iron or Bronze is wrong, so no chunk bounded by
// either may fill rather than show a fill measured from a doubtful bound.
TEST(MissingThresholdChunks, AnOutOfOrderThresholdHatchesEveryChunkItBounds) {
    const std::vector<Tier> tiers{Tier{TierId{"iron"}, "Iron", {}}, Tier{TierId{"bronze"}, "Bronze", {}},
                                  Tier{TierId{"silver"}, "Silver", {}}, Tier{TierId{"gold"}, "Gold", {}}};
    const std::vector<Threshold> thresholds{Threshold{TierId{"iron"}, 1800.0}, Threshold{TierId{"bronze"}, 1200.0},
                                            Threshold{TierId{"silver"}, 2400.0}, Threshold{TierId{"gold"}, 3000.0}};

    const ScoredScenario scored{thresholds, tiers};
    EXPECT_EQ(scored.missing(), (std::vector<bool>{true, true, true, false}))
        << "Iron's and Bronze's own chunks, and Silver's, which starts at Bronze";
    EXPECT_EQ(scored.fills(1995.0), (std::vector<double>{0.0, 0.0, 0.0, 0.0}))
        << "no chunk shows a fill measured from an out-of-order bound";
}

TEST(BenchmarkProgressGridModel, EdgeStatesUnplayedUnmatchedAndAboveTheTopTier) {
    auto fake = gridFake();
    auto &definition = fake->fixture.definition;
    // e1 (PB 800, Resolved) above Gold 700; e3 is Ambiguous; e2 has no runs of its linked scenario.
    definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 300.0}, Threshold{TierId{"t2"}, 500.0}, Threshold{TierId{"t3"}, 700.0}};
    fake->reevaluate();
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
    fake->fixture.runs = {runOn(ymd(2024, 3, 4), "h1", 700, 60)};
    fake->reevaluate();
    fake->publish();
    EXPECT_TRUE(rowFor(grid, "e1").value("edgeState").toString().isEmpty());
}

TEST(BenchmarkProgressGridModel, MissingThresholdChunksTravelWithTheRow) {
    auto fake = gridFake();
    fake->fixture.definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t3"}, 1200.0}};
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};

    const QVariantMap e1 = rowFor(vm.progressGrid(), "e1");
    QList<bool> missing;
    for (const QVariant &flag: e1.value("missingChunks").toList()) missing << flag.toBool();
    EXPECT_EQ(missing, (QList<bool>{false, true, true}));
    const QVariantList fills = e1.value("fills").toList();
    EXPECT_EQ(fills.at(0).toDouble(), 1.0) << "Bronze's chunk is bounded by 0 and its own threshold";
    EXPECT_EQ(fills.at(1).toDouble(), 0.0);
    EXPECT_EQ(fills.at(2).toDouble(), 0.0) << "no partial fill in a chunk without a usable bound";
}

TEST(BenchmarkProgressGridModel, RowsCarryTheScoreThatReachesEachTier) {
    auto fake = gridFake();
    // Gold has no threshold for e1: its text is empty, the others use the shared number format.
    fake->fixture.definition.categories[0].scenarios[0].thresholds = {
        Threshold{TierId{"t1"}, 600.0}, Threshold{TierId{"t2"}, 1234.5}};
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};

    const QVariantList texts = rowFor(vm.progressGrid(), "e1").value("thresholdTexts").toList();
    ASSERT_EQ(texts.size(), 3);
    EXPECT_EQ(texts.at(0).toString(), formatBenchmarkNumber(600.0));
    EXPECT_EQ(texts.at(1).toString(), formatBenchmarkNumber(1234.5));
    EXPECT_TRUE(texts.at(2).toString().isEmpty());
}

TEST(BenchmarkProgressGridModel, BlockingRowsNameTheNextTierInItsColour) {
    auto fake = gridFake();
    fake->fixture.definition.tiers[1].color = BenchmarkColor{192, 192, 200}; // Silver, the next tier
    auto &fixture = fake->fixture;
    fixture.definition.uncategorized[0].hashes = {"hu1"};
    fixture.definition.uncategorized[1].hashes = {"hu2"};
    fixture.resolutions[0] = resolvedEntry("u1");
    fixture.resolutions[1] = resolvedEntry("u2");
    fixture.resolutions.back() = resolvedEntry("e3");
    const auto day = ymd(2024, 3, 4);
    fixture.runs = {runOn(day, "hu1", 1000, 60), runOn(day, "hu2", 1000, 60),
                    runOn(day, "h1", 900, 60), runOn(day, "h3", 400, 60)};
    fake->reevaluate();
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    const QVariantMap e3 = rowFor(grid, "e3");
    EXPECT_TRUE(e3.value("blocker").toBool());
    EXPECT_EQ(e3.value("blockerText").toString(), QString("Blocks Silver (needs %1)").arg(formatBenchmarkNumber(600.0)));
    EXPECT_EQ(e3.value("blockerColor").toString(), QColor(192, 192, 200).name());
    for (const char *id: {"u1", "u2", "e1", "e2"}) {
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
    fake->snap.benchmark.reset();
    fake->snap.visiblePlaytime.clear();
    fake->publish();

    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(grid->rows().isEmpty());
    EXPECT_TRUE(grid->groups().isEmpty());
    EXPECT_TRUE(grid->tiers().isEmpty());
    EXPECT_FALSE(grid->hasSubcategories());
}

TEST(BenchmarkProgressGridModelTexts, NextTextJoinsTheTierAndItsThreshold) {
    auto fake = gridFake(breakdownWithTopTier());
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();

    const QVariantMap e1 = rowFor(grid, "e1");
    ASSERT_FALSE(e1.value("nextTierText").toString().isEmpty());
    EXPECT_EQ(e1.value("nextText").toString(),
              e1.value("nextTierText").toString() + " " + e1.value("nextThresholdText").toString());
    EXPECT_EQ(rowFor(grid, "e3").value("nextText").toString(), "Highest tier attained");
}

TEST(BenchmarkProgressGridModelTexts, PublishesEveryColumnHeading) {
    BenchmarkProgressGridModel grid;

    const QVariantMap headers = grid.property("headers").toMap();
    EXPECT_EQ(headers.size(), 7);
    for (const char *key: {"category", "subcategory", "scenario", "progress", "pbDate", "plays", "next"})
        EXPECT_FALSE(headers.value(key).toString().isEmpty()) << key;
}

TEST(BenchmarkProgressGridModel, UnadmittedThresholdsLeaveTheRowsTextBlank) {
    auto fake = gridFake();
    BenchmarkTrackingViewModel vm{fake};
    auto &thresholds = fake->fixture.definition.categories[0].scenarios[0].thresholds;
    const auto valid = thresholds;
    auto duplicate = valid;
    auto negative = valid;
    auto nonFinite = valid;
    duplicate.push_back({TierId{"t1"}, 510});
    negative[1].score = -1;
    nonFinite[2].score = std::numeric_limits<double>::infinity();
    const std::vector cases{duplicate, negative, nonFinite};
    for (qsizetype i = 0; i < static_cast<qsizetype>(cases.size()); ++i) {
        SCOPED_TRACE(i);
        thresholds = cases[static_cast<std::size_t>(i)];
        fake->reevaluate();
        fake->publish();

        const QVariantMap row = rowFor(vm.progressGrid(), "e1");
        const QVariantList texts = row.value("thresholdTexts").toList();
        const QVariantList missing = row.value("missingChunks").toList();
        ASSERT_EQ(texts.size(), 3);
        ASSERT_EQ(missing.size(), 3);
        EXPECT_TRUE(texts.at(i).toString().isEmpty());
        EXPECT_TRUE(missing.at(i).toBool());
        EXPECT_LT(row.value("recentPosition").toDouble(), 0.0);
    }
}

TEST(BenchmarkProgressGridModel, GroupRankChipsComeFromTheGroupEvaluation) {
    auto fake = gridFake(breakdownWithRankedGroups());
    BenchmarkTrackingViewModel vm{fake};
    const auto *grid = vm.progressGrid();
    const auto check = [&](const GroupEvaluation &group) {
        const auto &benchmark = *fake->benchmark();
        const auto tierId = group.satisfiedTier();
        const auto tier = std::ranges::find_if(benchmark.tiers(), [&](const Tier &candidate) {
            return tierId == candidate.id;
        });
        const QString expected = tier == benchmark.tiers().end()
                                     ? QStringLiteral("Unranked") : QString::fromStdString(tier->name);
        EXPECT_EQ(groupFor(grid, QString::fromStdString(group.id().value)).value("rankText").toString(), expected);
    };
    for (const bool played: {true, false}) {
        if (!played) {
            fake->fixture.runs.erase(fake->fixture.runs.begin());
            fake->reevaluate();
            fake->publish();
        }
        for (const auto *category: fake->benchmark()->categories()) {
            check(*category);
            for (const auto *subcategory: category->subgroups()) check(*subcategory);
        }
        EXPECT_TRUE(groupFor(grid, {}).value("rankText").toString().isEmpty());
    }
    fake->fixture.completeness = CompletenessResult{Completeness::Incomplete, {}};
    fake->reevaluate();
    fake->publish();
    for (const QVariant &group: grid->groups())
        EXPECT_TRUE(group.toMap().value("rankText").toString().isEmpty());
}
