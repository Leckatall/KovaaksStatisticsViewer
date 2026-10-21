#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "graph_configuration.h"
#include "series_config.h"

namespace ksv::application {
    enum class SeriesMutationFailureCode {
        UnknownSeriesId,
        DisplayPositionOutOfRange,
        ComputedSeriesIdExhausted,
        ConfigurationRejected,
        UnknownAxisId,
        AxisIdExhausted
    };

    struct MutationResult {
        std::vector<ValidationError> errors;
        std::optional<SeriesMutationFailureCode> failure;
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
        std::optional<uint32_t> hiddenLeadingPoints{};
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

    // Graph Lines edits as transformations of a caller-owned document. Each leaves the document
    // untouched unless the result succeeded.
    MutationResult createComputedSeries(RunSeriesDocument &document, const CreateComputedSeriesRequest &request);

    MutationResult updateSeries(RunSeriesDocument &document, const UpdateSeriesRequest &request);

    MutationResult removeSeries(RunSeriesDocument &document, SeriesId id);

    MutationResult reorderSeries(RunSeriesDocument &document, SeriesId id, uint32_t position);

    MutationResult createAxis(RunSeriesDocument &document, const CreateAxisRequest &request);

    MutationResult deleteAxis(RunSeriesDocument &document, AxisId id);
}
