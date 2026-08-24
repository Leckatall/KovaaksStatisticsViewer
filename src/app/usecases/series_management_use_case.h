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
        [[nodiscard]] std::vector<AxisConfig> getAllAxes() const override { return m_store->getAllAxes(); }
        MutationResult createAxis(const CreateAxisRequest &request) override { return m_store->createAxis(request); }
        MutationResult deleteAxis(const AxisId id) override { return m_store->deleteAxis(id); }
        void onChanged(std::function<void()> callback) override { m_store->onChanged(std::move(callback)); }

        void beginDraft() override { m_store->beginDraft(); }
        MutationResult commitDraft() override { return m_store->commitDraft(); }
        void discardDraft() override { m_store->discardDraft(); }
        [[nodiscard]] bool hasPendingChanges() const override { return m_store->hasPendingChanges(); }

    private:
        std::shared_ptr<ISeriesConfigStore> m_store;
    };
}
