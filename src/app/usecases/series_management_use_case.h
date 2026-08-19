#pragma once

#include <utility>

#include "contracts/i_series_management_use_case.h"

namespace ksv::application {
    class SeriesManagementUseCase final : public ISeriesManagementUseCase {
    public:
        explicit SeriesManagementUseCase(std::shared_ptr<ISeriesConfigStore> store) : m_store(std::move(store)) {}

        [[nodiscard]] std::vector<SeriesConfig> getAll() const override { return m_store->getAll(); }
        MutationResult setSeriesEnabled(const SeriesId id, const bool enabled) override {
            return m_store->updateSeries({.id = id, .presentation = UpdatedSeriesPresentation{.enabled = enabled}});
        }
        MutationResult createComputed(const CreateComputedSeriesRequest &request) override { return m_store->createComputed(request); }
        MutationResult updateSeries(const UpdateSeriesRequest &request) override { return m_store->updateSeries(request); }
        MutationResult removeComputed(const SeriesId id) override { return m_store->removeComputed(id); }
        MutationResult reorder(const SeriesId id, const uint32_t position) override { return m_store->reorder(id, position); }
        void onChanged(std::function<void()> callback) override { m_store->onChanged(std::move(callback)); }

    private:
        std::shared_ptr<ISeriesConfigStore> m_store;
    };
}
