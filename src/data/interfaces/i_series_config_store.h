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
        UnknownAxisId,
        AxisIdExhausted
    };

    struct MutationResult {
        std::vector<ValidationError> errors;
        std::optional<StoreMutationFailureCode> failure;
        bool requiresReload = false;
        std::optional<SeriesId> createdId;
        std::optional<AxisId> createdAxisId;

        [[nodiscard]] bool succeeded() const { return errors.empty() && !failure.has_value(); }
    };

    struct CreateComputedSeriesRequest {
        SeriesPresentation presentation;
        Expression expression;
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
        // Outer optional: whether this patch touches yAxisId at all. Inner optional: the new value,
        // which may itself be null (independent axis).
        std::optional<std::optional<AxisId>> yAxisId;
    };

    struct CreateAxisRequest {
        std::string name;
    };

    class ISeriesConfigStore {
    public:
        virtual ~ISeriesConfigStore() = default;
        [[nodiscard]] virtual std::vector<SeriesConfig> getAll() const = 0;
        virtual MutationResult createComputed(const CreateComputedSeriesRequest &) = 0;
        virtual MutationResult updateSeries(const UpdateSeriesRequest &) = 0;
        virtual MutationResult removeComputed(SeriesId) = 0;
        virtual MutationResult reorder(SeriesId, uint32_t position) = 0;
        virtual void onChanged(std::function<void()> callback) = 0;

        [[nodiscard]] virtual std::vector<AxisConfig> getAllAxes() const = 0;
        virtual MutationResult createAxis(const CreateAxisRequest &) = 0;
        virtual MutationResult deleteAxis(AxisId) = 0;

        virtual void beginDraft() = 0;
        virtual MutationResult commitDraft() = 0;
        virtual void discardDraft() = 0;
        [[nodiscard]] virtual bool hasPendingChanges() const = 0;
    };
}
