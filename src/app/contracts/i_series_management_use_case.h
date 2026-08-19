#pragma once

#include <functional>
#include <vector>

#include "series_config.h"
#include "data/interfaces/i_series_config_store.h"

namespace ksv::application {
    class ISeriesManagementUseCase {
    public:
        virtual ~ISeriesManagementUseCase() = default;

        [[nodiscard]] virtual std::vector<SeriesConfig> getAll() const = 0;
        virtual MutationResult setSeriesEnabled(SeriesId, bool) = 0;
        virtual MutationResult createComputed(const CreateComputedSeriesRequest &) = 0;
        virtual MutationResult updateSeries(const UpdateSeriesRequest &) = 0;
        virtual MutationResult removeComputed(SeriesId) = 0;
        virtual MutationResult reorder(SeriesId, uint32_t position) = 0;
        virtual void onChanged(std::function<void()> callback) = 0;
    };
}
