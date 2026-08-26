#include <gtest/gtest.h>

#include "usecases/series_management_use_case.h"
#include "fake_series_config_store.h"

using namespace ksv::application;
using namespace ksv::tests_support;

namespace {
    TEST(SeriesManagementUseCaseTest, GetAllForwardsEveryRowRegardlessOfEnabled) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        store->configs = {
            SeriesConfig{{1}, {"Enabled", {}, true, 0}, primitive(PrimitiveMetric::Score)},
            SeriesConfig{{2}, {"Disabled", {}, false, 1}, primitive(PrimitiveMetric::Shots)},
        };
        SeriesManagementUseCase useCase(store);
        EXPECT_EQ(useCase.getAll().size(), 2U);
    }

    TEST(SeriesManagementUseCaseTest, GetAllAxesForwardsFromStore) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        store->axes = {AxisConfig{AxisId{1}, "Accuracy", {}, AxisTransformKind::Percentage}};
        SeriesManagementUseCase useCase(store);

        EXPECT_EQ(useCase.getAllAxes().size(), 1U);
    }

    TEST(SeriesManagementUseCaseTest, CreateAxisDelegatesToStore) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        SeriesManagementUseCase useCase(store);

        useCase.createAxis({"Custom"});

        EXPECT_EQ(store->createAxisCalls, 1);
    }

    TEST(SeriesManagementUseCaseTest, DeleteAxisDelegatesToStore) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        SeriesManagementUseCase useCase(store);

        useCase.deleteAxis(AxisId{3});

        EXPECT_EQ(store->deleteAxisCalls, 1);
    }

    TEST(SeriesManagementUseCaseTest, SetSeriesEnabledUpdatesOnlyThePresentationEnabledField) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        SeriesManagementUseCase useCase(store);
        useCase.setSeriesEnabled(SeriesId{7}, true);
        ASSERT_TRUE(store->lastUpdateSeriesRequest);
        EXPECT_EQ(store->lastUpdateSeriesRequest->id.value, 7U);
        ASSERT_TRUE(store->lastUpdateSeriesRequest->presentation);
        ASSERT_TRUE(store->lastUpdateSeriesRequest->presentation->enabled);
        EXPECT_TRUE(*store->lastUpdateSeriesRequest->presentation->enabled);
        EXPECT_FALSE(store->lastUpdateSeriesRequest->presentation->name);
        EXPECT_FALSE(store->lastUpdateSeriesRequest->expression);
    }

    TEST(SeriesManagementUseCaseTest, CreateUpdateRemoveReorderAllForwardDirectlyToTheStore) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        SeriesManagementUseCase useCase(store);
        useCase.createComputed({{"Custom", {}, true}, numericConstant(1.0)});
        EXPECT_EQ(store->createComputedCalls, 1);
        useCase.removeComputed(SeriesId{3});
        EXPECT_EQ(store->lastRemovedId->value, 3U);
        useCase.reorder(SeriesId{4}, 2);
        EXPECT_EQ(store->lastReorderedId->value, 4U);
        EXPECT_EQ(store->lastReorderPosition, 2U);
    }

    TEST(SeriesManagementUseCaseTest, OnChangedForwardsStoreNotifications) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        SeriesManagementUseCase useCase(store);
        int notifications = 0;
        useCase.onChanged([&] { ++notifications; });
        ASSERT_EQ(store->callbacks.size(), 1U);
        store->callbacks.front()();
        EXPECT_EQ(notifications, 1);
    }

    TEST(SeriesManagementUseCaseTest, DraftLifecycleAndPendingChangesForwardDirectlyToTheStore) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        SeriesManagementUseCase useCase(store);

        useCase.beginDraft();
        EXPECT_EQ(store->beginDraftCalls, 1);

        store->pendingChanges = true;
        EXPECT_TRUE(useCase.hasPendingChanges());

        useCase.commitDraft();
        EXPECT_EQ(store->commitDraftCalls, 1);

        useCase.discardDraft();
        EXPECT_EQ(store->discardDraftCalls, 1);
    }
}
