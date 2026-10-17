// BenchmarkTrackingViewModel adapts one IBenchmarkTrackingUseCase revision into the workspace's
// selector, state text, status summary, progress grid, and child graph models. These tests drive it
// through a single fake use case (never a service) and assert on the QML-facing surface.

#include <gtest/gtest.h>

#include <QColor>
#include <QLocale>
#include <QSignalSpy>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <cstddef>
#include <memory>
#include <vector>

#include "presentation/benchmark_tracking_vm.h"
#include "presentation/benchmark_history_vm.h"
#include "presentation/graph_vm_base.h"
#include "presentation/series_model.h"
#include "presentation/benchmark_issue_text.h"
#include "presentation/benchmark_format.h"

#include "fake_benchmark_tracking_use_case.h"

using namespace ksv::presentation;
using namespace ksv::application;
using namespace ksv::domain;
using ksv::tests_ui::FakeBenchmarkTrackingUseCase;
using ksv::tests_ui::makeTrackableSnapshot;
using ksv::tests_ui::vtChoice;

namespace {
    // The sole drawn series in each history child; column ids on a GraphViewModelBase carry no
    // meaning outside the class, so tests reference the raw id directly (see graph_vm_test.cpp).
    constexpr int kValueColumn = 1;

    std::shared_ptr<FakeBenchmarkTrackingUseCase> fakeWith(BenchmarkWorkspaceSnapshot snapshot) {
        auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
        fake->snap = std::move(snapshot);
        return fake;
    }
}

TEST(BenchmarkTrackingViewModel, ConstructsFromUseCaseAloneAndForwardsSelectionCommands) {
    auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
    BenchmarkTrackingViewModel vm{fake};

    vm.selectBenchmark(QStringLiteral("b1"));
    vm.clearSelection();

    ASSERT_EQ(fake->selectCalls.size(), 1u);
    EXPECT_EQ(fake->selectCalls.front().value, "b1");
    EXPECT_EQ(fake->clearCalls, 1);
}

TEST(BenchmarkTrackingViewModel, SelectorOrdersLoadedEntriesFirstThenProblemsWithNameAndFilenameSort) {
    BenchmarkWorkspaceSnapshot snapshot;
    snapshot.availability = BenchmarkTrackingState::NoSelection;
    snapshot.choices = {
        vtChoice("m_banana.json", std::string{"b"}, "banana",
                 BenchmarkChoiceClassification::Trackable, true),
        vtChoice("a_apple.json", std::string{"a1"}, "Apple",
                 BenchmarkChoiceClassification::Incomplete, true),
        vtChoice("z_apple.json", std::string{"a2"}, "apple",
                 BenchmarkChoiceClassification::Trackable, true),
        vtChoice("p_bad.json", std::nullopt, "", BenchmarkChoiceClassification::Invalid, false),
        vtChoice("c_bad.json", std::nullopt, "", BenchmarkChoiceClassification::Unsupported, false),
    };
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto entries = vm.selectorEntries();
    ASSERT_EQ(entries.size(), 5);
    // Loaded first, locale-aware case-insensitive name sort, filename as the tie-break.
    EXPECT_EQ(entries.at(0).toMap().value("name").toString(), "Apple");
    EXPECT_EQ(entries.at(1).toMap().value("name").toString(), "apple");
    EXPECT_EQ(entries.at(2).toMap().value("name").toString(), "banana");
    EXPECT_TRUE(entries.at(0).toMap().value("selectable").toBool());
    // Problem entries follow, keyed by filename, and stay non-selectable but visible.
    EXPECT_EQ(entries.at(3).toMap().value("name").toString(), "c_bad.json");
    EXPECT_EQ(entries.at(4).toMap().value("name").toString(), "p_bad.json");
    EXPECT_FALSE(entries.at(3).toMap().value("selectable").toBool());
    EXPECT_FALSE(entries.at(4).toMap().value("selectable").toBool());
    // The selector labels every non-trackable entry by this classification string.
    EXPECT_EQ(entries.at(0).toMap().value("classification").toString(), "incomplete");
    EXPECT_EQ(entries.at(1).toMap().value("classification").toString(), "trackable");
    EXPECT_EQ(entries.at(3).toMap().value("classification").toString(), "unsupported");
    EXPECT_EQ(entries.at(4).toMap().value("classification").toString(), "invalid");
}

TEST(BenchmarkTrackingViewModel, SelectorOrdersRecentLoadedEntriesFirstThenAlphabeticalThenProblems) {
    const auto withRecency = [](BenchmarkChoice choice, const std::size_t recency) {
        choice.recency = recency;
        return choice;
    };
    BenchmarkWorkspaceSnapshot snapshot;
    snapshot.availability = BenchmarkTrackingState::NoSelection;
    snapshot.choices = {
        withRecency(vtChoice("m_banana.json", std::string{"b"}, "banana",
                             BenchmarkChoiceClassification::Trackable, true), 1),
        vtChoice("a_apple.json", std::string{"a1"}, "Apple",
                 BenchmarkChoiceClassification::Incomplete, true),
        vtChoice("z_apple.json", std::string{"a2"}, "apple",
                 BenchmarkChoiceClassification::Trackable, true),
        withRecency(vtChoice("d_cherry.json", std::string{"c"}, "Cherry",
                             BenchmarkChoiceClassification::Trackable, true), 0),
        withRecency(vtChoice("a_problem.json", std::nullopt, "",
                             BenchmarkChoiceClassification::Invalid, false), 0),
        vtChoice("c_bad.json", std::nullopt, "", BenchmarkChoiceClassification::Unsupported, false),
    };
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const auto entries = vm.selectorEntries();
    ASSERT_EQ(entries.size(), 6);
    // Recency ascending leads; unrecorded loaded entries follow alphabetically; problems come last by
    // filename even when they carry a recency.
    std::vector<QString> filenames;
    for (const auto &entry: entries) filenames.push_back(entry.toMap().value("filename").toString());
    EXPECT_EQ(filenames, (std::vector<QString>{
        "d_cherry.json", "m_banana.json", "a_apple.json", "z_apple.json", "a_problem.json", "c_bad.json"}));
}

TEST(BenchmarkTrackingViewModel, EmptyLibraryStateExposesInstallGuidance) {
    BenchmarkWorkspaceSnapshot snapshot;
    snapshot.availability = BenchmarkTrackingState::NoSelection;
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.state(), "emptyLibrary");
    EXPECT_FALSE(vm.stateMessage().isEmpty());
}

TEST(BenchmarkTrackingViewModel, NoSelectionStateWhenChoicesExistButNoneChosen) {
    BenchmarkWorkspaceSnapshot snapshot;
    snapshot.availability = BenchmarkTrackingState::NoSelection;
    snapshot.choices = {vtChoice("b1.json", std::string{"b1"}, "Voltaic",
                                 BenchmarkChoiceClassification::Trackable, true)};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.state(), "noSelection");
    EXPECT_FALSE(vm.stateMessage().isEmpty());
}

TEST(BenchmarkTrackingViewModel, UnavailableStateRetainsLastKnownNameAndClearsDerivedData) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.availability = BenchmarkTrackingState::Unavailable;
    snapshot.lastKnownSelectedDisplayName = "Voltaic S3";
    snapshot.selectedLoaded.reset();
    snapshot.projection.reset();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.state(), "unavailable");
    EXPECT_EQ(vm.selectedName(), "Voltaic S3");
    EXPECT_FALSE(vm.summaryAvailable());
    EXPECT_TRUE(vm.progressGrid()->rows().isEmpty());
    EXPECT_TRUE(vm.rankHistory()->series({kValueColumn}).isEmpty());
    EXPECT_TRUE(vm.playtimeHistory()->series({kValueColumn}).isEmpty());
}

TEST(BenchmarkTrackingViewModel, TrackableSummaryResolvesRanksAndFormatsAverageAndPlaytime) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.state(), "trackable");
    EXPECT_TRUE(vm.summaryAvailable());
    EXPECT_EQ(vm.attainedRank(), "Silver");
    EXPECT_EQ(vm.completedRank(), "None completed");
    EXPECT_EQ(vm.nextTier(), "Gold");
    EXPECT_EQ(vm.averageRank(), "Silver · 50% to Gold");
    EXPECT_EQ(vm.totalPlaytime(), "1h 1m");
    EXPECT_EQ(vm.scenariosAtNextTier(), 1);
    EXPECT_TRUE(vm.hasNextTier());
    EXPECT_EQ(vm.nextTierBlockerCount(), 2);
}

TEST(BenchmarkTrackingViewModel, SummaryFallsBackWhenNothingAttainedCompletedOrHigher) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->attainedRank.reset();
    snapshot.projection->completedRank.reset();
    snapshot.projection->nextTier.reset();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.attainedRank(), "Unranked");
    EXPECT_EQ(vm.completedRank(), "None completed");
    EXPECT_EQ(vm.nextTier(), "Highest tier attained");
    EXPECT_FALSE(vm.hasNextTier());
    EXPECT_EQ(vm.nextTierBlockerCount(), 0);
}

TEST(BenchmarkTrackingViewModel, TotalPlaytimeUsesMinuteAndHourThresholds) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto playtimeFor = [&](const double seconds) {
        fake->snap.projection->totalPlaytimeSeconds = seconds;
        fake->publish();
        return vm.totalPlaytime();
    };

    EXPECT_EQ(playtimeFor(45.0), "<1 min");
    EXPECT_EQ(playtimeFor(125.0), "2 min");
    EXPECT_EQ(playtimeFor(3600.0), "1h 0m");
    EXPECT_EQ(playtimeFor(7320.0), "2h 2m");
}

// The fixture ladder is Bronze, Silver, Gold: rank 0 is Unranked and k is the k-th tier.
TEST(BenchmarkTrackingViewModel, AverageRankReadsAsTheTierAndProgressTowardTheNext) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto averageFor = [&](const double rank) {
        fake->snap.projection->averageRank = rank;
        fake->publish();
        return vm.averageRank();
    };
    // Mid-tier: the fraction is progress toward the next tier.
    EXPECT_EQ(averageFor(1.6), "Bronze · 60% to Silver");
    EXPECT_EQ(averageFor(2.5), "Silver · 50% to Gold");
    // Below the first tier.
    EXPECT_EQ(averageFor(0.25), "Unranked · 25% to Bronze");
    EXPECT_EQ(averageFor(0.0), "Unranked · 0% to Bronze");
    // An exact tier boundary has attained that tier and no progress yet.
    EXPECT_EQ(averageFor(2.0), "Silver · 0% to Gold");
    // A mean that lands a rounding error short of a boundary is the boundary, not 99%.
    EXPECT_EQ(averageFor(2.0 - 1e-12), "Silver · 0% to Gold");
    // The percentage never reads 100% before the next tier is reached.
    EXPECT_EQ(averageFor(1.9999), "Bronze · 99% to Silver");
    // The top tier has nothing to progress toward.
    EXPECT_EQ(averageFor(3.0), "Gold · top tier reached");
}

TEST(BenchmarkTrackingViewModel, IncompleteDefinitionShowsIssuesAndSuppressesRankDerivedValues) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.selectedLoaded->completeness = CompletenessResult{
        Completeness::Incomplete,
        {BenchmarkIssue{BenchmarkIssueCode::NoTiers, std::monostate{}},
         BenchmarkIssue{BenchmarkIssueCode::MissingThreshold, std::monostate{}}}};
    snapshot.projection->completeness = Completeness::Incomplete;
    snapshot.projection->attainedRank.reset();
    snapshot.projection->completedRank.reset();
    snapshot.projection->nextTier.reset();
    snapshot.projection->averageRank.reset();
    snapshot.projection->nextTierBlockers.clear();
    snapshot.projection->averageRankHistory.clear();
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.state(), "incomplete");

    const QStringList issues = vm.completenessIssues();
    EXPECT_EQ(issues.size(), 2);
    EXPECT_TRUE(issues.contains(benchmarkIssueText(BenchmarkIssueCode::NoTiers)));
    EXPECT_TRUE(issues.contains(benchmarkIssueText(BenchmarkIssueCode::MissingThreshold)));

    EXPECT_FALSE(vm.summaryAvailable());
    // Never a plausible-looking value while setup is incomplete.
    EXPECT_NE(vm.attainedRank(), "Unranked");
    EXPECT_NE(vm.attainedRank(), "0");
    EXPECT_NE(vm.averageRank(), "0");
    for (const QVariant &row: vm.progressGrid()->rows())
        EXPECT_FALSE(row.toMap().value("blocker").toBool()) << "no blocker without an official rank";
    EXPECT_TRUE(vm.rankHistory()->series({kValueColumn}).isEmpty());
    // Playtime is a safe fact the evaluator can still interpret.
    EXPECT_FALSE(vm.playtimeHistory()->series({kValueColumn}).isEmpty());
}

// The workspace's progress heading reads hasNextTier and nextTierBlockerCount. A benchmark that is
// not trackable has no next tier and no blockers, even when its projection still carries them (as
// it does here, so the check fails if the view model forwards them regardless of the state).
TEST(BenchmarkTrackingViewModel, NonTrackableBenchmarkHasNoNextTierAndNoBlockers) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    ASSERT_EQ(vm.state(), "trackable");
    ASSERT_TRUE(vm.hasNextTier());
    ASSERT_GT(vm.nextTierBlockerCount(), 0);

    fake->snap.selectedLoaded->completeness = CompletenessResult{
        Completeness::Incomplete, {BenchmarkIssue{BenchmarkIssueCode::MissingThreshold, std::monostate{}}}};
    fake->snap.projection->completeness = Completeness::Incomplete;
    ASSERT_TRUE(fake->snap.projection->nextTier.has_value());
    ASSERT_FALSE(fake->snap.projection->nextTierBlockers.empty());
    fake->publish();

    EXPECT_EQ(vm.state(), "incomplete");
    EXPECT_FALSE(vm.hasNextTier());
    EXPECT_EQ(vm.nextTierBlockerCount(), 0);
}

TEST(BenchmarkTrackingViewModel, BlockerCountEqualsTheRowsTheGridMarks) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    int marked = 0;
    for (const auto &row: vm.progressGrid()->rows())
        if (row.toMap().value("blocker").toBool()) ++marked;
    EXPECT_EQ(vm.nextTierBlockerCount(), marked);
}

TEST(BenchmarkTrackingViewModel, ProgressGridListsEveryScenarioAndMarksTheBlockers) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const QVariantList rows = vm.progressGrid()->rows();
    ASSERT_EQ(rows.size(), 2);
    // Uncategorized first, then the categories, as the definition orders them.
    const auto pasu = rows.at(0).toMap();
    EXPECT_EQ(pasu.value("name").toString(), "pasu");
    EXPECT_TRUE(pasu.value("blocker").toBool());
    EXPECT_TRUE(pasu.value("blockerText").toString().startsWith("Blocks Gold"));
    EXPECT_TRUE(pasu.value("blockerText").toString().contains("900"));

    const auto wall = rows.at(1).toMap();
    EXPECT_EQ(wall.value("name").toString(), "1w6ts");
    EXPECT_TRUE(wall.value("blocker").toBool());
    EXPECT_TRUE(wall.value("personalBestText").toString().contains("800"));
}

TEST(BenchmarkTrackingViewModel, ReadyToUnavailableClearsSummaryGridAndGraphsBeforeNotifying) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    ASSERT_FALSE(vm.progressGrid()->rows().isEmpty());
    ASSERT_FALSE(vm.rankHistory()->series({kValueColumn}).isEmpty());

    QString stateInNotification;
    int rowsInNotification = -1;
    bool rankLineEmptyInNotification = false;
    QObject::connect(&vm, &BenchmarkTrackingViewModel::changed, [&] {
        stateInNotification = vm.state();
        rowsInNotification = static_cast<int>(vm.progressGrid()->rows().size());
        rankLineEmptyInNotification = vm.rankHistory()->series({kValueColumn}).isEmpty();
    });
    const QSignalSpy spy(&vm, &BenchmarkTrackingViewModel::changed);

    fake->snap.availability = BenchmarkTrackingState::Unavailable;
    fake->snap.selectedLoaded.reset();
    fake->snap.projection.reset();
    fake->publish();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(stateInNotification, "unavailable");
    EXPECT_EQ(rowsInNotification, 0);
    EXPECT_TRUE(rankLineEmptyInNotification);
}

// the workspace asks for a default benchmark; the view model only forwards the request
// and leaves the choice (remembered or sole benchmark) to the use case.
TEST(BenchmarkTrackingViewModel, SelectDefaultBenchmarkForwardsToTheUseCase) {
    auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
    BenchmarkTrackingViewModel vm{fake};

    vm.selectDefaultBenchmark();

    EXPECT_EQ(fake->selectDefaultCalls, 1);
    EXPECT_TRUE(fake->selectCalls.empty());
}

// the summary's rank names carry their tier's readable colour; non-tier values do not.
TEST(BenchmarkTrackingViewModel, SummaryRankNamesCarryTheirTierColours) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->tiers[1].color = BenchmarkColor{192, 192, 200}; // Silver (attained)
    snapshot.projection->tiers[2].color = BenchmarkColor{255, 200, 40};  // Gold (next)
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(vm.attainedRankColor(), QColor(192, 192, 200).name());
    EXPECT_EQ(vm.nextTierColor(), QColor(255, 200, 40).name());
    EXPECT_TRUE(vm.completedRankColor().isEmpty()) << "'None completed' is not a tier";

    fake->snap.availability = BenchmarkTrackingState::Unavailable;
    fake->snap.selectedLoaded.reset();
    fake->snap.projection.reset();
    fake->publish();
    EXPECT_TRUE(vm.attainedRankColor().isEmpty());
    EXPECT_TRUE(vm.nextTierColor().isEmpty());
}

TEST(BenchmarkTrackingViewModel, DarkTierColoursAreLightenedForReadability) {
    auto snapshot = makeTrackableSnapshot();
    snapshot.projection->tiers[1].color = BenchmarkColor{10, 10, 10};
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    const QColor shown(vm.attainedRankColor());
    ASSERT_TRUE(shown.isValid());
    EXPECT_GE(contrastRatio(shown, darkPanelColor()), 4.5);
}

// --- the summary's average-rank bar --------------------------------------------------------------

namespace {
    std::vector<double> listToFills(const QVariantList &list) {
        std::vector<double> fills;
        for (const QVariant &value: list) fills.push_back(value.toDouble());
        return fills;
    }
}

// averageRank is the mean normalized rank: exactly k fills k whole chunks, so 6.37 is six full
// chunks and 37% of the seventh.
TEST(RankChunkFills, WholeRanksEndAtTheirChunksRightEdgeAndFractionsFillTheNext) {
    EXPECT_EQ(rankChunkFills(3.5, 5), (std::vector<double>{1, 1, 1, 0.5, 0}));
    EXPECT_EQ(rankChunkFills(3.0, 5), (std::vector<double>{1, 1, 1, 0, 0}));
    EXPECT_EQ(rankChunkFills(0.0, 5), (std::vector<double>{0, 0, 0, 0, 0}));
    EXPECT_EQ(rankChunkFills(5.0, 5), (std::vector<double>{1, 1, 1, 1, 1}));
    const auto low = rankChunkFills(0.87, 5);
    EXPECT_NEAR(low[0], 0.87, 1e-9) << "below the first tier: part of Iron's chunk";
    EXPECT_EQ(low[1], 0.0);
}

TEST(BenchmarkTrackingViewModel, AverageRankBarFillsAndCompletedRankTick) {
    auto snapshot = makeTrackableSnapshot(); // Bronze, Silver, Gold; average 2.5; nothing completed
    auto fake = fakeWith(snapshot);
    BenchmarkTrackingViewModel vm{fake};

    EXPECT_EQ(listToFills(vm.averageRankFills()), (std::vector<double>{1, 1, 0.5}));
    EXPECT_EQ(vm.completedRankPosition(), -1.0);

    fake->snap.projection->completedRank = TierId{"t2"};
    fake->publish();
    EXPECT_EQ(vm.completedRankPosition(), 2.0) << "the tick ends Silver's chunk";

    fake->snap.availability = BenchmarkTrackingState::Unavailable;
    fake->snap.selectedLoaded.reset();
    fake->snap.projection.reset();
    fake->publish();
    EXPECT_TRUE(vm.averageRankFills().isEmpty());
    EXPECT_EQ(vm.completedRankPosition(), -1.0);
}
