// BenchmarkTrackingViewModel adapts one IBenchmarkTrackingUseCase revision into the workspace's
// selector, state text, status summary, blockers, and child graph models. These tests drive it
// through a single fake use case (never a service) and assert on the QML-facing surface.

#include <gtest/gtest.h>

#include <QLocale>
#include <QSignalSpy>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "presentation/benchmark_tracking_vm.h"
#include "presentation/benchmark_history_vm.h"
#include "presentation/graph_vm_base.h"
#include "presentation/series_model.h"
#include "presentation/benchmark_issue_text.h"

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
    EXPECT_TRUE(vm.blockers().isEmpty());
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
    EXPECT_EQ(vm.averageRank(), QLocale().toString(2.5));
    EXPECT_EQ(vm.totalPlaytime(), "1h 1m");
    EXPECT_EQ(vm.scenariosAtNextTier(), 1);
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

TEST(BenchmarkTrackingViewModel, AverageRankUsesLocaleWithAtMostTwoFractionDigitsAndNoTrailingZeroes) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    fake->snap.projection->averageRank = 2.5;
    fake->publish();
    EXPECT_EQ(vm.averageRank(), QLocale().toString(2.5));

    fake->snap.projection->averageRank = 2.718;
    fake->publish();
    EXPECT_EQ(vm.averageRank(), QLocale().toString(2.72));
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
    EXPECT_TRUE(vm.blockers().isEmpty());
    EXPECT_TRUE(vm.rankHistory()->series({kValueColumn}).isEmpty());
    // Playtime is a safe fact the evaluator can still interpret.
    EXPECT_FALSE(vm.playtimeHistory()->series({kValueColumn}).isEmpty());
}

TEST(BenchmarkTrackingViewModel, BlockerListResolvesScenarioAndGroupNamesWithCurrentAndRequired) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};

    const auto blockers = vm.blockers();
    ASSERT_EQ(blockers.size(), 2);

    const auto first = blockers.at(0).toMap();
    EXPECT_EQ(first.value("scenarioName").toString(), "1w6ts");
    EXPECT_EQ(first.value("groupName").toString(), "Clicking");
    EXPECT_TRUE(first.value("currentText").toString().contains("800"));
    EXPECT_TRUE(first.value("requiredText").toString().contains("850"));

    const auto second = blockers.at(1).toMap();
    EXPECT_EQ(second.value("scenarioName").toString(), "pasu");
    EXPECT_EQ(second.value("groupName").toString(), QString());
    EXPECT_EQ(second.value("currentText").toString(), "Unplayed");
    EXPECT_TRUE(second.value("requiredText").toString().contains("900"));
}

TEST(BenchmarkTrackingViewModel, ReadyToUnavailableClearsSummaryBlockersAndGraphsBeforeNotifying) {
    auto fake = fakeWith(makeTrackableSnapshot());
    BenchmarkTrackingViewModel vm{fake};
    ASSERT_FALSE(vm.blockers().isEmpty());
    ASSERT_FALSE(vm.rankHistory()->series({kValueColumn}).isEmpty());

    QString stateInNotification;
    int blockersInNotification = -1;
    bool rankLineEmptyInNotification = false;
    QObject::connect(&vm, &BenchmarkTrackingViewModel::changed, [&] {
        stateInNotification = vm.state();
        blockersInNotification = static_cast<int>(vm.blockers().size());
        rankLineEmptyInNotification = vm.rankHistory()->series({kValueColumn}).isEmpty();
    });
    const QSignalSpy spy(&vm, &BenchmarkTrackingViewModel::changed);

    fake->snap.availability = BenchmarkTrackingState::Unavailable;
    fake->snap.selectedLoaded.reset();
    fake->snap.projection.reset();
    fake->publish();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(stateInNotification, "unavailable");
    EXPECT_EQ(blockersInNotification, 0);
    EXPECT_TRUE(rankLineEmptyInNotification);
}
