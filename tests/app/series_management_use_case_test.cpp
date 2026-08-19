#include <gtest/gtest.h>

#include "usecases/series_management_use_case.h"

using namespace ksv::application;

namespace {
    class FakeSeriesConfigStore final : public ISeriesConfigStore {
    public:
        [[nodiscard]] std::vector<SeriesConfig> getAll() const override { return configs; }
        MutationResult createComputed(const CreateComputedSeriesRequest &) override { ++createComputedCalls; return {}; }
        MutationResult updateComputed(const UpdateComputedSeriesRequest &) override { return {}; }
        MutationResult updateBase(const UpdateBaseSeriesRequest &) override { return {}; }
        MutationResult updateSeries(const UpdateSeriesRequest &request) override { lastUpdateSeriesRequest = request; return {}; }
        MutationResult removeComputed(const SeriesId id) override { lastRemovedId = id; return {}; }
        MutationResult reorder(const SeriesId id, const uint32_t position) override {
            lastReorderedId = id;
            lastReorderPosition = position;
            return {};
        }
        void onChanged(std::function<void()> callback) override { callbacks.push_back(std::move(callback)); }

        std::vector<SeriesConfig> configs;
        int createComputedCalls = 0;
        std::optional<UpdateSeriesRequest> lastUpdateSeriesRequest;
        std::optional<SeriesId> lastRemovedId;
        std::optional<SeriesId> lastReorderedId;
        uint32_t lastReorderPosition = 0;
        std::vector<std::function<void()>> callbacks;
    };

    TEST(SeriesManagementUseCaseTest, GetAllForwardsEveryRowRegardlessOfEnabled) {
        const auto store = std::make_shared<FakeSeriesConfigStore>();
        store->configs = {
            SeriesConfig{{1}, {"Enabled", {}, true, 0}, primitive(PrimitiveMetric::Score)},
            SeriesConfig{{2}, {"Disabled", {}, false, 1}, primitive(PrimitiveMetric::Shots)},
        };
        SeriesManagementUseCase useCase(store);
        EXPECT_EQ(useCase.getAll().size(), 2U);
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
}
