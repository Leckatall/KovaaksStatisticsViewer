#pragma once

#include <functional>
#include <optional>
#include <variant>

#include "app/contracts/series_config.h"

namespace ksv::application {
    enum class StoreMutationFailureCode {
        UnknownComputedSeriesId, InvalidPrimitiveMetric, DisplayPositionOutOfRange,
        ComputedSeriesIdExhausted, PersistenceWriteFailed
    };

    struct MutationResult {
        std::vector<ValidationError> errors;
        std::optional<StoreMutationFailureCode> failure;
        bool requiresReload = false;
        std::optional<ComputedSeriesId> createdId;

        [[nodiscard]] bool succeeded() const { return errors.empty() && !failure.has_value(); }
    };

    struct ComputedSeriesPresentation {
        std::string name;
        LineStyle lineStyle;
        bool enabled;
    };

    struct CreateComputedSeriesRequest {
        ComputedSeriesPresentation presentation;
        Expression expression;
    };

    struct UpdateComputedSeriesRequest {
        ComputedSeriesId id;
        ComputedSeriesPresentation presentation;
        Expression expression;
    };

    struct UpdateBaseSeriesRequest {
        PrimitiveMetric metric;
        bool enabled;
        LineStyle lineStyle;
    };

    using SeriesRecordReference = std::variant<PrimitiveMetric, ComputedSeriesId>;

    class ISeriesConfigStore {
    public:
        virtual ~ISeriesConfigStore() = default;
        [[nodiscard]] virtual std::vector<SeriesConfig> getAll() const = 0;
        virtual MutationResult createComputed(const CreateComputedSeriesRequest &) = 0;
        virtual MutationResult updateComputed(const UpdateComputedSeriesRequest &) = 0;
        virtual MutationResult updateBase(const UpdateBaseSeriesRequest &) = 0;
        virtual MutationResult removeComputed(ComputedSeriesId) = 0;
        virtual MutationResult reorder(SeriesRecordReference, uint32_t position) = 0;
        virtual void onChanged(std::function<void()> callback) = 0;
    };
}
