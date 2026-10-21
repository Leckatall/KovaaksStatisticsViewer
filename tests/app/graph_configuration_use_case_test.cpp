#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "data/graph_configuration_service.h"
#include "usecases/graph_configuration_use_case.h"
#include "fake_graph_configuration_persistence.h"

using namespace ksv;
using namespace ksv::application;
using ksv::tests_support::FakeGraphConfigurationPersistence;

namespace {
    class GraphConfigurationUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphConfigurationPersistence> persistence =
            std::make_shared<FakeGraphConfigurationPersistence>();
        std::shared_ptr<data::GraphConfigurationService> service =
            std::make_shared<data::GraphConfigurationService>(persistence);
        GraphConfigurationUseCase useCase{service};
        std::vector<GraphConfigurationChange> changes;

        void SetUp() override {
            useCase.onChanged([this](const GraphConfigurationChange &change) { changes.push_back(change); });
        }

        [[nodiscard]] const GraphPreferences &committed(const GraphKey key) const {
            return useCase.current().configuration->preferences(key);
        }
    };

    TEST_F(GraphConfigurationUseCaseTest, CurrentReflectsTheCommittedSnapshot) {
        const auto current = useCase.current();

        EXPECT_EQ(*current.configuration, defaultGraphConfiguration());
        EXPECT_EQ(current.revision, 1U);
    }

    TEST_F(GraphConfigurationUseCaseTest, SavingAValidCandidateCommitsWritesAndPublishesIt) {
        auto candidate = *useCase.current().configuration;
        candidate.preferences(GraphKey::BenchmarkRank).range = RangePreference{RecentDays{30}, false};
        candidate.runAxisLabelSeriesId = SeriesId{3};

        const auto result = useCase.save(candidate);

        EXPECT_TRUE(result.accepted());
        EXPECT_EQ(result.revision, 2U);
        EXPECT_EQ(*useCase.current().configuration, candidate);
        ASSERT_EQ(persistence->saves.size(), 1U);
        EXPECT_EQ(persistence->saves.front(), candidate);
        ASSERT_EQ(changes.size(), 1U);
        auto expected = GraphConfigurationChange{2, false, true};
        expected.sections[static_cast<std::size_t>(GraphKey::BenchmarkRank)].range = true;
        EXPECT_EQ(changes.front(), expected);
    }

    // EDGE-02: an invalid field on a graph other than the one being edited still blocks the whole save,
    // and the valid edits beside it are not partially applied.
    TEST_F(GraphConfigurationUseCaseTest, AnInvalidFieldOnAnyGraphRejectsTheWholeCandidate) {
        auto candidate = *useCase.current().configuration;
        candidate.preferences(GraphKey::ScenarioHistory).range = RangePreference{LatestRuns{50}, false};
        candidate.preferences(GraphKey::BenchmarkPlaytime).axisPolicies["playtime"] = FixedBounds{10.0, 5.0};

        const auto result = useCase.save(candidate);

        EXPECT_FALSE(result.accepted());
        EXPECT_EQ(result.revision, 1U);
        EXPECT_EQ(result.issues, (std::vector{GraphConfigurationIssue{
                      GraphConfigurationIssueCode::FixedBoundsNotIncreasing, GraphKey::BenchmarkPlaytime, "axes",
                      "playtime", "fixedMax"}}));
        EXPECT_EQ(*useCase.current().configuration, defaultGraphConfiguration());
        EXPECT_TRUE(persistence->saves.empty());
        EXPECT_TRUE(changes.empty());
    }

    TEST_F(GraphConfigurationUseCaseTest, AQuickPatchReplacesOnlyItsGraphsPatchedFields) {
        GraphQuickPatch patch;
        patch.range = RangePreference{RecentDays{30}, false};
        patch.connection = ConnectionMode::Straight;
        patch.seriesStyles["accuracy"] = SeriesStyleOverride{RgbaColor{1, 2, 3, 255}, 5.0};
        patch.axisPolicies["score"] = FixedBounds{0.0, 1000.0};

        const auto result = useCase.applyQuickPatch(GraphKey::ScenarioHistory, patch);

        EXPECT_TRUE(result.accepted());
        auto expected = defaultGraphConfiguration();
        auto &history = expected.preferences(GraphKey::ScenarioHistory);
        history.range = RangePreference{RecentDays{30}, false};
        history.appearance.connection = ConnectionMode::Straight;
        history.seriesStyles["accuracy"] = SeriesStyleOverride{RgbaColor{1, 2, 3, 255}, 5.0};
        history.axisPolicies["score"] = FixedBounds{0.0, 1000.0};
        EXPECT_EQ(*useCase.current().configuration, expected);
        ASSERT_EQ(persistence->saves.size(), 1U);
        ASSERT_EQ(changes.size(), 1U);
        EXPECT_EQ(std::ranges::count_if(changes.front().sections, &GraphSectionChange::any), 1);
        EXPECT_TRUE(changes.front().sectionsOf(GraphKey::ScenarioHistory).any());
    }

    TEST_F(GraphConfigurationUseCaseTest, AnInvalidQuickPatchChangesNothing) {
        const auto before = useCase.current();
        GraphQuickPatch invalidWindow;
        invalidWindow.connection = ConnectionMode::PointsOnly;
        invalidWindow.averagingWindowDays = 0;
        GraphQuickPatch inapplicableRange;
        inapplicableRange.range = RangePreference{RecentDays{7}, false};
        GraphQuickPatch unknownSeries;
        unknownSeries.seriesStyles["kills"] = SeriesStyleOverride{std::nullopt, 4.0};

        const auto window = useCase.applyQuickPatch(GraphKey::DailyPlaytime, invalidWindow);
        const auto range = useCase.applyQuickPatch(GraphKey::Run, inapplicableRange);
        const auto series = useCase.applyQuickPatch(GraphKey::ScenarioHistory, unknownSeries);

        ASSERT_EQ(window.issues.size(), 1U);
        EXPECT_EQ(window.issues.front().code, GraphConfigurationIssueCode::NonPositiveCount);
        EXPECT_EQ(window.issues.front().graph, GraphKey::DailyPlaytime);
        ASSERT_EQ(range.issues.size(), 1U);
        EXPECT_EQ(range.issues.front().code, GraphConfigurationIssueCode::NotApplicable);
        ASSERT_EQ(series.issues.size(), 1U);
        EXPECT_EQ(series.issues.front().code, GraphConfigurationIssueCode::UnknownSeries);
        EXPECT_EQ(useCase.current().configuration, before.configuration);
        EXPECT_EQ(useCase.current().revision, before.revision);
        EXPECT_TRUE(persistence->saves.empty());
        EXPECT_TRUE(changes.empty());
    }

    TEST_F(GraphConfigurationUseCaseTest, AnEmptySeriesOverrideRestoresThatSeriesDefaults) {
        GraphQuickPatch widen;
        widen.seriesStyles["playtime"] = SeriesStyleOverride{std::nullopt, 6.0};
        ASSERT_TRUE(useCase.applyQuickPatch(GraphKey::BenchmarkPlaytime, widen).accepted());
        GraphQuickPatch restore;
        restore.seriesStyles["playtime"] = SeriesStyleOverride{};

        ASSERT_TRUE(useCase.applyQuickPatch(GraphKey::BenchmarkPlaytime, restore).accepted());

        EXPECT_TRUE(committed(GraphKey::BenchmarkPlaytime).seriesStyles.empty());
        EXPECT_EQ(*useCase.current().configuration, defaultGraphConfiguration());
    }

    TEST_F(GraphConfigurationUseCaseTest, QuickPatchesAccumulateAndSeedTheNextSettingsVisit) {
        const auto newWindow = *committed(GraphKey::DailyPlaytime).averagingWindowDays + 1;
        const auto newMarkers = !committed(GraphKey::DailyPlaytime).appearance.lineModeMarkers;
        GraphQuickPatch window;
        window.averagingWindowDays = newWindow;
        GraphQuickPatch markers;
        markers.lineModeMarkers = newMarkers;
        ASSERT_TRUE(useCase.applyQuickPatch(GraphKey::DailyPlaytime, window).accepted());
        ASSERT_TRUE(useCase.applyQuickPatch(GraphKey::DailyPlaytime, markers).accepted());

        const auto seed = useCase.current();

        EXPECT_EQ(seed.revision, 3U);
        EXPECT_EQ(seed.configuration->preferences(GraphKey::DailyPlaytime).averagingWindowDays, newWindow);
        EXPECT_EQ(seed.configuration->preferences(GraphKey::DailyPlaytime).appearance.lineModeMarkers, newMarkers);
    }

    TEST(GraphConfigurationDiff, NamesEachChangedGraphAndSectionInKeyOrder) {
        const auto before = defaultGraphConfiguration();
        auto after = before;
        after.preferences(GraphKey::BenchmarkPlaytime).averagingWindowDays = 5;
        after.preferences(GraphKey::Run).appearance.connection = ConnectionMode::Straight;
        after.runSeries.nextAxisId = AxisId{9};

        auto expected = GraphConfigurationChange{0, true, false};
        expected.sections[static_cast<std::size_t>(GraphKey::Run)].style = true;
        expected.sections[static_cast<std::size_t>(GraphKey::BenchmarkPlaytime)].window = true;
        EXPECT_EQ(diffGraphConfigurations(before, after), expected);
        EXPECT_TRUE(diffGraphConfigurations(before, before).empty());
    }
}
