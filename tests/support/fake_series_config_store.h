#ifndef KOVAAKSSTATSVIEWER_TESTS_FAKE_SERIES_CONFIG_STORE_H
#define KOVAAKSSTATSVIEWER_TESTS_FAKE_SERIES_CONFIG_STORE_H

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "data/interfaces/i_series_config_store.h"

namespace ksv::tests_support {
    class FakeSeriesConfigStore final : public application::ISeriesConfigStore {
    public:
        std::vector<application::SeriesConfig> configs;
        std::vector<application::AxisConfig> axes;
        int createComputedCalls = 0;
        int createAxisCalls = 0;
        int deleteAxisCalls = 0;
        int beginDraftCalls = 0;
        int commitDraftCalls = 0;
        int discardDraftCalls = 0;
        bool pendingChanges = false;
        std::optional<application::UpdateSeriesRequest> lastUpdateSeriesRequest;
        std::optional<application::SeriesId> lastRemovedId;
        std::optional<application::SeriesId> lastReorderedId;
        uint32_t lastReorderPosition = 0;
        std::vector<std::function<void()>> callbacks;

        [[nodiscard]] std::vector<application::SeriesConfig> getAll() const override { return configs; }
        application::MutationResult createComputed(const application::CreateComputedSeriesRequest &) override {
            ++createComputedCalls;
            notify();
            return {};
        }
        application::MutationResult updateSeries(const application::UpdateSeriesRequest &request) override {
            lastUpdateSeriesRequest = request;
            notify();
            return {};
        }
        application::MutationResult removeComputed(const application::SeriesId id) override {
            lastRemovedId = id;
            notify();
            return {};
        }
        application::MutationResult reorder(const application::SeriesId id, const uint32_t position) override {
            lastReorderedId = id;
            lastReorderPosition = position;
            notify();
            return {};
        }
        void onChanged(std::function<void()> callback) override { callbacks.push_back(std::move(callback)); }

        [[nodiscard]] std::vector<application::AxisConfig> getAllAxes() const override { return axes; }
        application::MutationResult createAxis(const application::CreateAxisRequest &) override {
            ++createAxisCalls;
            notify();
            return {};
        }
        application::MutationResult deleteAxis(application::AxisId) override {
            ++deleteAxisCalls;
            notify();
            return {};
        }

        void beginDraft() override { ++beginDraftCalls; }
        application::MutationResult commitDraft() override { ++commitDraftCalls; return {}; }
        void discardDraft() override { ++discardDraftCalls; }
        [[nodiscard]] bool hasPendingChanges() const override { return pendingChanges; }

    private:
        void notify() const { for (const auto &callback: callbacks) callback(); }

    };
}

#endif
