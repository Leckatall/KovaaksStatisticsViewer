#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "data/graph_configuration_service.h"
#include "fake_graph_configuration_persistence.h"

using namespace ksv;
using namespace ksv::application;
using ksv::data::GraphConfigurationService;
using ksv::tests_support::FakeGraphConfigurationPersistence;

namespace {
    static_assert(std::is_same_v<decltype(GraphConfigurationSnapshot::configuration),
                                 std::shared_ptr<const GraphConfiguration>>);

    class GraphConfigurationServiceTest : public testing::Test {
    protected:
        std::shared_ptr<FakeGraphConfigurationPersistence> persistence =
            std::make_shared<FakeGraphConfigurationPersistence>();

        [[nodiscard]] static GraphConfiguration withTwoGraphChanges() {
            auto candidate = defaultGraphConfiguration();
            candidate.preferences(GraphKey::DailyPlaytime).averagingWindowDays = 7;
            candidate.preferences(GraphKey::ScenarioHistory).appearance.connection = ConnectionMode::Straight;
            return candidate;
        }
    };

    TEST_F(GraphConfigurationServiceTest, LoadsTheCommittedSnapshotOnceAtConstruction) {
        persistence->stored.preferences(GraphKey::BenchmarkRank).range = RangePreference{RecentDays{30}, true};

        const GraphConfigurationService service(persistence);

        EXPECT_EQ(persistence->loads, 1);
        EXPECT_EQ(*service.snapshot().configuration, persistence->stored);
        EXPECT_EQ(service.snapshot().revision, 1U);
        EXPECT_TRUE(persistence->saves.empty());
    }

    TEST_F(GraphConfigurationServiceTest, AcceptedCandidateIsWrittenOnceThenPublishedWithItsChangedGraphs) {
        GraphConfigurationService service(persistence);
        const auto candidate = withTwoGraphChanges();
        std::vector<GraphConfigurationChange> changes;
        service.onChanged([&](const GraphConfigurationChange &change) {
            // Publication follows the whole replacement: the snapshot is already the new one.
            const auto snapshot = service.snapshot();
            EXPECT_EQ(*snapshot.configuration, candidate);
            EXPECT_EQ(snapshot.revision, change.revision);
            EXPECT_EQ(persistence->saves.size(), 1U);
            changes.push_back(change);
        });

        const auto result = service.replace(candidate);

        EXPECT_TRUE(result.accepted());
        EXPECT_EQ(result.revision, 2U);
        ASSERT_EQ(persistence->saves.size(), 1U);
        EXPECT_EQ(persistence->saves.front(), candidate);
        ASSERT_EQ(changes.size(), 1U);
        EXPECT_EQ(changes.front(), (GraphConfigurationChange{
                      2, {GraphKey::DailyPlaytime, GraphKey::ScenarioHistory}, false, false}));
    }

    TEST_F(GraphConfigurationServiceTest, NamesTheRunSeriesAndAxisLabelSections) {
        GraphConfigurationService service(persistence);
        auto candidate = defaultGraphConfiguration();
        candidate.runSeries.series.pop_back();
        candidate.runAxisLabelSeriesId = SeriesId{2};
        std::vector<GraphConfigurationChange> changes;
        service.onChanged([&](const GraphConfigurationChange &change) { changes.push_back(change); });

        ASSERT_TRUE(service.replace(candidate).accepted());

        ASSERT_EQ(changes.size(), 1U);
        EXPECT_EQ(changes.front(), (GraphConfigurationChange{2, {}, true, true}));
    }

    TEST_F(GraphConfigurationServiceTest, RejectedCandidateReplacesWritesAndPublishesNothing) {
        GraphConfigurationService service(persistence);
        const auto before = service.snapshot();
        auto candidate = withTwoGraphChanges();
        candidate.preferences(GraphKey::BenchmarkPlaytime).averagingWindowDays = 0;
        int notifications = 0;
        service.onChanged([&](const GraphConfigurationChange &) { ++notifications; });

        const auto result = service.replace(candidate);

        EXPECT_FALSE(result.accepted());
        EXPECT_EQ(result.revision, 1U);
        ASSERT_EQ(result.issues.size(), 1U);
        EXPECT_EQ(result.issues.front().graph, GraphKey::BenchmarkPlaytime);
        EXPECT_EQ(result.issues.front().field, "averagingWindow");
        EXPECT_EQ(service.snapshot().configuration, before.configuration);
        EXPECT_EQ(service.snapshot().revision, 1U);
        EXPECT_TRUE(persistence->saves.empty());
        EXPECT_EQ(notifications, 0);
    }

    TEST_F(GraphConfigurationServiceTest, UnchangedCandidateIsAcceptedWithoutAWriteOrPublication) {
        GraphConfigurationService service(persistence);
        int notifications = 0;
        service.onChanged([&](const GraphConfigurationChange &) { ++notifications; });

        const auto result = service.replace(defaultGraphConfiguration());

        EXPECT_TRUE(result.accepted());
        EXPECT_EQ(result.revision, 1U);
        EXPECT_TRUE(persistence->saves.empty());
        EXPECT_EQ(notifications, 0);
    }

    TEST_F(GraphConfigurationServiceTest, AHeldSnapshotKeepsItsContentAfterAReplacement) {
        GraphConfigurationService service(persistence);
        const auto held = service.snapshot();

        ASSERT_TRUE(service.replace(withTwoGraphChanges()).accepted());

        EXPECT_EQ(*held.configuration, defaultGraphConfiguration());
        EXPECT_NE(service.snapshot().configuration, held.configuration);
    }

    // An observer may react by replacing again. Every observer must still see the changes in revision
    // order rather than the nested change overtaking the one still being delivered.
    TEST_F(GraphConfigurationServiceTest, AReplacementFromACallbackIsPublishedAfterTheCurrentChange) {
        GraphConfigurationService service(persistence);
        std::vector<std::pair<char, std::uint64_t>> deliveries;
        service.onChanged([&](const GraphConfigurationChange &change) {
            deliveries.emplace_back('a', change.revision);
            if (change.revision == 2) {
                auto again = *service.snapshot().configuration;
                again.preferences(GraphKey::Run).appearance.connection = ConnectionMode::PointsOnly;
                EXPECT_TRUE(service.replace(again).accepted());
            }
        });
        service.onChanged([&](const GraphConfigurationChange &change) { deliveries.emplace_back('b', change.revision); });

        ASSERT_TRUE(service.replace(withTwoGraphChanges()).accepted());

        EXPECT_EQ(deliveries, (std::vector<std::pair<char, std::uint64_t>>{{'a', 2}, {'b', 2}, {'a', 3}, {'b', 3}}));
        EXPECT_EQ(service.snapshot().revision, 3U);
        EXPECT_EQ(persistence->saves.size(), 2U);
    }
}
