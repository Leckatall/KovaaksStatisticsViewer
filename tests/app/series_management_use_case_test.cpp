#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "data/graph_configuration_service.h"
#include "usecases/graph_configuration_use_case.h"
#include "usecases/series_management_use_case.h"
#include "fake_graph_configuration_persistence.h"

using namespace ksv;
using namespace ksv::application;
using ksv::tests_support::FakeGraphConfigurationPersistence;

namespace {
    class SeriesManagementUseCaseTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphConfigurationPersistence> persistence =
            std::make_shared<FakeGraphConfigurationPersistence>();
        std::shared_ptr<GraphConfigurationUseCase> configuration = std::make_shared<GraphConfigurationUseCase>(
            std::make_shared<data::GraphConfigurationService>(persistence));
        SeriesManagementUseCase useCase{configuration};
        int committedChanges = 0;
        int editorNotifications = 0;

        void SetUp() override {
            configuration->onChanged([this](const GraphConfigurationChange &) { ++committedChanges; });
            useCase.onChanged([this] { ++editorNotifications; });
        }

        [[nodiscard]] std::vector<SeriesConfig> committedSeries() const {
            return configuration->current().configuration->runSeries.series;
        }

        [[nodiscard]] static bool contains(const std::vector<SeriesConfig> &series, const SeriesId id) {
            return std::ranges::find(series, id, &SeriesConfig::id) != series.end();
        }

        static CreateComputedSeriesRequest request() {
            return {{"Custom", {{1, 2, 3, 255}, 2.0}, true}, numericConstant(4.0)};
        }
    };

    TEST_F(SeriesManagementUseCaseTest, ReadsEveryCommittedRowRegardlessOfEnabled) {
        ASSERT_TRUE(useCase.setSeriesEnabled(SeriesId{2}, false).succeeded());

        EXPECT_EQ(useCase.getAll().size(), 9U);
        EXPECT_EQ(useCase.getAllAxes().size(), 2U);
        EXPECT_FALSE(committedSeries()[1].presentation.enabled);
    }

    TEST_F(SeriesManagementUseCaseTest, SetSeriesEnabledChangesOnlyThatRowsEnabledFlag) {
        auto expected = committedSeries();
        expected[6].presentation.enabled = false;

        ASSERT_TRUE(useCase.setSeriesEnabled(SeriesId{7}, false).succeeded());

        EXPECT_TRUE(sameSeriesConfigs(committedSeries(), expected));
    }

    TEST_F(SeriesManagementUseCaseTest, WithoutAnOpenDraftEachEditIsSavedAtOnce) {
        const auto created = useCase.createComputed(request());

        ASSERT_TRUE(created.succeeded());
        EXPECT_TRUE(contains(committedSeries(), *created.createdId));
        EXPECT_EQ(persistence->saves.size(), 1U);
        EXPECT_EQ(committedChanges, 1);
        EXPECT_EQ(editorNotifications, 1);
    }

    // STATE-01/SCOPE-03: Graph Lines edits made during a settings visit must not reach the committed
    // snapshot the dashboard draws from, nor its change publication, until the visit saves.
    TEST_F(SeriesManagementUseCaseTest, DraftEditsStayOutOfTheCommittedSnapshotUntilCommitted) {
        useCase.beginDraft();
        const auto created = useCase.createComputed(request());
        ASSERT_TRUE(useCase.createAxis({"Draft axis"}).succeeded());

        EXPECT_TRUE(contains(useCase.getAll(), *created.createdId));
        EXPECT_EQ(useCase.getAllAxes().size(), 3U);
        EXPECT_FALSE(contains(committedSeries(), *created.createdId));
        EXPECT_TRUE(persistence->saves.empty());
        EXPECT_EQ(committedChanges, 0);
        EXPECT_EQ(editorNotifications, 2);
        EXPECT_TRUE(useCase.hasPendingChanges());

        ASSERT_TRUE(useCase.commitDraft().succeeded());

        EXPECT_TRUE(contains(committedSeries(), *created.createdId));
        EXPECT_EQ(configuration->current().configuration->runSeries.axes.size(), 3U);
        EXPECT_EQ(persistence->saves.size(), 1U);
        EXPECT_EQ(committedChanges, 1);
    }

    TEST_F(SeriesManagementUseCaseTest, CommittingKeepsTheVisitOpenAndClean) {
        useCase.beginDraft();
        ASSERT_TRUE(useCase.createComputed(request()).succeeded());
        ASSERT_TRUE(useCase.commitDraft().succeeded());
        EXPECT_FALSE(useCase.hasPendingChanges());

        const auto later = useCase.createComputed(request());

        EXPECT_TRUE(useCase.hasPendingChanges());
        EXPECT_FALSE(contains(committedSeries(), *later.createdId));
        EXPECT_EQ(persistence->saves.size(), 1U);
    }

    TEST_F(SeriesManagementUseCaseTest, DiscardRestoresTheCommittedRowsAndNotifiesOnce) {
        const auto before = useCase.getAll();
        useCase.beginDraft();
        ASSERT_TRUE(useCase.updateSeries({.id = SeriesId{1}, .yAxisId = AxisId{2}}).succeeded());
        editorNotifications = 0;

        useCase.discardDraft();

        EXPECT_FALSE(useCase.hasPendingChanges());
        EXPECT_TRUE(sameSeriesConfigs(useCase.getAll(), before));
        EXPECT_EQ(editorNotifications, 1);
        EXPECT_TRUE(persistence->saves.empty());
        EXPECT_EQ(committedChanges, 0);
    }

    // SettingsDialog.open() begins a draft on every open, including focusing the already-open window.
    TEST_F(SeriesManagementUseCaseTest, ReopeningAVisitWithUnsavedEditsKeepsThem) {
        useCase.beginDraft();
        const auto created = useCase.createComputed(request());

        useCase.beginDraft();

        EXPECT_TRUE(useCase.hasPendingChanges());
        EXPECT_TRUE(contains(useCase.getAll(), *created.createdId));
    }

    TEST_F(SeriesManagementUseCaseTest, CommittingTheDraftKeepsPreferencesCommittedDuringTheVisit) {
        useCase.beginDraft();
        const auto created = useCase.createComputed(request());
        GraphQuickPatch patch;
        patch.averagingWindowDays = 9;
        ASSERT_TRUE(configuration->applyQuickPatch(GraphKey::DailyPlaytime, patch).accepted());

        ASSERT_TRUE(useCase.commitDraft().succeeded());

        const auto committed = configuration->current().configuration;
        EXPECT_EQ(committed->preferences(GraphKey::DailyPlaytime).averagingWindowDays, 9);
        EXPECT_TRUE(contains(committed->runSeries.series, *created.createdId));
    }

    TEST_F(SeriesManagementUseCaseTest, AFailedEditLeavesTheDraftAsItWas) {
        useCase.beginDraft();
        const auto before = useCase.getAll();
        editorNotifications = 0;

        EXPECT_EQ(useCase.removeComputed(SeriesId{99}).failure, SeriesMutationFailureCode::UnknownSeriesId);
        EXPECT_FALSE(useCase.createComputed({{" ", {}, true}, numericConstant(1)}).succeeded());

        EXPECT_TRUE(sameSeriesConfigs(useCase.getAll(), before));
        EXPECT_FALSE(useCase.hasPendingChanges());
        EXPECT_EQ(editorNotifications, 0);
    }

    TEST_F(SeriesManagementUseCaseTest, CommitOrDiscardWithoutADraftDoesNothing) {
        EXPECT_TRUE(useCase.commitDraft().succeeded());
        useCase.discardDraft();

        EXPECT_FALSE(useCase.hasPendingChanges());
        EXPECT_TRUE(persistence->saves.empty());
        EXPECT_EQ(editorNotifications, 0);
    }
}
