#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

#include "capture/capture_sources.h"
#include "presentation/benchmarks/tracking/benchmark_progress_grid_model.h"
#include "presentation/benchmarks/tracking/benchmark_tracking_vm.h"

#include "../benchmarks/tracking/fake_benchmark_tracking_use_case.h"

namespace ksv::ui {
    namespace {
        using namespace ksv::application;
        using namespace ksv::domain;
        using presentation::BenchmarkTrackingViewModel;
        using presentation::ProgressGridContent;
        using tests_ui::FakeBenchmarkTrackingUseCase;
        using tests_ui::makeBreakdownSnapshot;

        std::shared_ptr<FakeBenchmarkTrackingUseCase> fakeWith(BenchmarkWorkspaceSnapshot snapshot) {
            auto fake = std::make_shared<FakeBenchmarkTrackingUseCase>();
            fake->snap = std::move(snapshot);
            return fake;
        }

        std::vector<QString> rowNames(const ProgressGridContent &grid) {
            std::vector<QString> names;
            for (const auto &row: grid.rows) names.push_back(row.name);
            return names;
        }

        std::vector<QString> personalBestTexts(const ProgressGridContent &grid) {
            std::vector<QString> texts;
            for (const auto &row: grid.rows) texts.push_back(row.personalBestText);
            return texts;
        }
    }

    TEST(CaptureTableSource, CopiesTheGridAndSummaryOfATrackableBenchmark) {
        auto fake = fakeWith(makeBreakdownSnapshot());
        BenchmarkTrackingViewModel vm{fake};
        ASSERT_EQ(vm.state(), "trackable");
        ASSERT_TRUE(vm.summaryAvailable());
        const ProgressGridContent live = vm.progressGrid()->content();

        const auto snapshot = captureTable(vm, CaptureAppearance{});

        ASSERT_TRUE(snapshot.has_value());
        EXPECT_EQ(snapshot->benchmarkName, vm.selectedName());
        EXPECT_EQ(snapshot->grid.rows.size(), live.rows.size());
        EXPECT_EQ(snapshot->grid.tiers.size(), live.tiers.size());
        EXPECT_EQ(snapshot->grid.groups.size(), live.groups.size());
        ASSERT_EQ(snapshot->grid.rows.size(), live.rows.size());
        for (std::size_t i = 0; i < live.rows.size(); ++i) {
            EXPECT_EQ(snapshot->grid.rows[i].name, live.rows[i].name);
            EXPECT_EQ(snapshot->grid.rows[i].thresholdTexts, live.rows[i].thresholdTexts);
        }
        ASSERT_TRUE(snapshot->summary.has_value());
        EXPECT_EQ(snapshot->summary->captions.attainedRank, vm.summaryCaptions().value("attainedRank").toString());
        EXPECT_EQ(snapshot->summary->captions.nextTier, vm.summaryCaptions().value("nextTier").toString());
        EXPECT_EQ(snapshot->summary->attainedRank, vm.attainedRank());
        EXPECT_EQ(snapshot->summary->completedRank, vm.completedRank());
        EXPECT_EQ(snapshot->summary->averageRank, vm.averageRank());
        EXPECT_EQ(snapshot->summary->completedRankPosition, vm.completedRankPosition());
        EXPECT_EQ(snapshot->summary->totalPlaytime, vm.totalPlaytime());
        EXPECT_EQ(snapshot->summary->nextTier, vm.nextTier());
        EXPECT_EQ(snapshot->summary->nextTierBlockerText, vm.nextTierBlockerText());
        const QVariantList fills = vm.averageRankFills();
        ASSERT_EQ(snapshot->summary->averageRankFills.size(), static_cast<std::size_t>(fills.size()));
        for (qsizetype i = 0; i < fills.size(); ++i)
            EXPECT_EQ(snapshot->summary->averageRankFills[static_cast<std::size_t>(i)], fills.at(i).toDouble());
        EXPECT_TRUE(snapshot->completenessNote.isEmpty());
    }

    TEST(CaptureTableSource, AnIncompleteBenchmarkHasANoteAndNoSummary) {
        auto incomplete = makeBreakdownSnapshot();
        incomplete.selectedLoaded->completeness = CompletenessResult{
            Completeness::Incomplete, {BenchmarkIssue{BenchmarkIssueCode::MissingThreshold, std::monostate{}}}};
        incomplete.projection->completeness = Completeness::Incomplete;
        incomplete.projection->attainedRank.reset();
        incomplete.projection->nextTier.reset();
        incomplete.projection->averageRank.reset();
        incomplete.projection->nextTierBlockers.clear();
        auto fake = fakeWith(incomplete);
        BenchmarkTrackingViewModel vm{fake};
        ASSERT_EQ(vm.state(), "incomplete");
        ASSERT_FALSE(vm.progressGrid()->content().rows.empty());

        const auto snapshot = captureTable(vm, CaptureAppearance{});

        ASSERT_TRUE(snapshot.has_value());
        EXPECT_FALSE(snapshot->summary.has_value());
        EXPECT_FALSE(snapshot->completenessNote.isEmpty());
    }

    TEST(CaptureTableSource, NoUsableTableIsUnavailable) {
        BenchmarkWorkspaceSnapshot noSelection;
        noSelection.availability = BenchmarkTrackingState::NoSelection;
        auto noSelectionFake = fakeWith(noSelection);
        BenchmarkTrackingViewModel noSelectionVm{noSelectionFake};
        ASSERT_EQ(noSelectionVm.state(), "emptyLibrary");
        EXPECT_FALSE(captureTable(noSelectionVm, CaptureAppearance{}).has_value());

        auto unavailable = makeBreakdownSnapshot();
        unavailable.availability = BenchmarkTrackingState::Unavailable;
        unavailable.selectedLoaded.reset();
        unavailable.projection.reset();
        auto unavailableFake = fakeWith(unavailable);
        BenchmarkTrackingViewModel unavailableVm{unavailableFake};
        ASSERT_EQ(unavailableVm.state(), "unavailable");
        EXPECT_FALSE(captureTable(unavailableVm, CaptureAppearance{}).has_value());

        auto empty = makeBreakdownSnapshot();
        empty.selectedLoaded->benchmark.categories.clear();
        empty.selectedLoaded->benchmark.uncategorized.clear();
        empty.projection->categories.clear();
        empty.projection->uncategorized.clear();
        auto emptyFake = fakeWith(empty);
        BenchmarkTrackingViewModel emptyVm{emptyFake};
        ASSERT_EQ(emptyVm.state(), "trackable");
        ASSERT_TRUE(emptyVm.progressGrid()->content().rows.empty());
        EXPECT_FALSE(captureTable(emptyVm, CaptureAppearance{}).has_value());
    }

    TEST(CaptureTableSource, SurvivesALaterRefresh) {
        auto fake = fakeWith(makeBreakdownSnapshot());
        BenchmarkTrackingViewModel vm{fake};
        const auto snapshot = captureTable(vm, CaptureAppearance{});
        ASSERT_TRUE(snapshot.has_value());
        ASSERT_TRUE(snapshot->summary.has_value());
        const std::vector<QString> names = rowNames(snapshot->grid);
        const std::vector<QString> personalBests = personalBestTexts(snapshot->grid);
        const TableCaptureSummary summary = *snapshot->summary;

        fake->snap = tests_ui::makeTrackableSnapshot();
        fake->publish();
        ASSERT_NE(rowNames(vm.progressGrid()->content()), names);

        EXPECT_EQ(rowNames(snapshot->grid), names);
        EXPECT_EQ(personalBestTexts(snapshot->grid), personalBests);
        EXPECT_EQ(snapshot->summary->attainedRank, summary.attainedRank);
        EXPECT_EQ(snapshot->summary->averageRank, summary.averageRank);
        EXPECT_EQ(snapshot->summary->totalPlaytime, summary.totalPlaytime);
        EXPECT_EQ(snapshot->summary->averageRankFills, summary.averageRankFills);
    }
}
