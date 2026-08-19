#pragma once

#include <functional>
#include <optional>
#include <variant>

#include "app/contracts/series_config.h"

namespace ksv::application {
    enum class StoreMutationFailureCode {
        UnknownSeriesId,
        DisplayPositionOutOfRange,
        ComputedSeriesIdExhausted,
        PersistenceWriteFailed,
        BannedPrimitiveUpdateType
    };

    struct MutationResult {
        std::vector<ValidationError> errors;
        std::optional<StoreMutationFailureCode> failure;
        bool requiresReload = false;
        std::optional<SeriesId> createdId;

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
        SeriesId id;
        ComputedSeriesPresentation presentation;
        Expression expression;
    };

    struct UpdateBaseSeriesRequest {
        SeriesId id;
        bool enabled;
        LineStyle lineStyle;
    };

    struct UpdatedSeriesPresentation {
        std::optional<std::string> name{};
        std::optional<LineStyle> lineStyle{};
        std::optional<bool> enabled{};
    };

    struct UpdateSeriesRequest {
        SeriesId id;
        std::optional<UpdatedSeriesPresentation> presentation;
        std::optional<Expression> expression;
    };

    class ISeriesConfigStore {
    public:
        virtual ~ISeriesConfigStore() = default;
        [[nodiscard]] virtual std::vector<SeriesConfig> getAll() const = 0;
        virtual MutationResult createComputed(const CreateComputedSeriesRequest &) = 0;
        virtual MutationResult updateComputed(const UpdateComputedSeriesRequest &) = 0;
        virtual MutationResult updateBase(const UpdateBaseSeriesRequest &) = 0;
        virtual MutationResult updateSeries(const UpdateSeriesRequest &) = 0;
        virtual MutationResult removeComputed(SeriesId) = 0;
        virtual MutationResult reorder(SeriesId, uint32_t position) = 0;
        virtual void onChanged(std::function<void()> callback) = 0;
    };
}
