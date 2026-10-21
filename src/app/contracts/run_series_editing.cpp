#include "run_series_editing.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

namespace ksv::application {
    namespace {
        void normalizeDisplayPositions(std::vector<SeriesConfig> &series) {
            for (size_t index = 0; index < series.size(); ++index)
                series[index].presentation.displayPosition = static_cast<uint32_t>(index);
        }

        [[nodiscard]] std::optional<size_t> indexOf(const RunSeriesDocument &document, const SeriesId id) {
            const auto found = std::ranges::find(document.series, id, &SeriesConfig::id);
            if (found == document.series.end()) return std::nullopt;
            return static_cast<size_t>(std::distance(document.series.begin(), found));
        }

        MutationResult install(RunSeriesDocument &document, RunSeriesDocument edited,
                               const std::optional<SeriesId> createdId = std::nullopt,
                               const std::optional<AxisId> createdAxisId = std::nullopt) {
            auto errors = validateSeriesConfigs(edited.series);
            if (!errors.empty()) return {std::move(errors)};
            document = std::move(edited);
            return {{}, std::nullopt, false, createdId, createdAxisId};
        }

        template<typename Id>
        [[nodiscard]] std::optional<Id> successor(const Id id) {
            if (id.value == std::numeric_limits<uint64_t>::max()) return std::nullopt;
            return Id{id.value + 1};
        }
    }

    MutationResult createComputedSeries(RunSeriesDocument &document, const CreateComputedSeriesRequest &request) {
        if (!document.nextSeriesId) return {{}, SeriesMutationFailureCode::ComputedSeriesIdExhausted};
        auto edited = document;
        const auto id = *edited.nextSeriesId;
        auto presentation = request.presentation;
        presentation.displayPosition = static_cast<uint32_t>(edited.series.size());
        edited.series.push_back(SeriesConfig{id, std::move(presentation), request.expression});
        edited.nextSeriesId = successor(id);
        return install(document, std::move(edited), id);
    }

    MutationResult updateSeries(RunSeriesDocument &document, const UpdateSeriesRequest &request) {
        const auto index = indexOf(document, request.id);
        if (!index) return {{}, SeriesMutationFailureCode::UnknownSeriesId};
        auto edited = document;
        auto &config = edited.series[*index];
        if (request.presentation) {
            const auto &[name, lineStyle, enabled, hiddenLeadingPoints] = *request.presentation;
            if (hiddenLeadingPoints) config.presentation.hiddenLeadingPoints = *hiddenLeadingPoints;
            if (name) config.presentation.name = *name;
            if (lineStyle) config.presentation.lineStyle = *lineStyle;
            if (enabled) config.presentation.enabled = *enabled;
        }
        if (request.expression) config.expression = *request.expression;
        if (request.yAxisId) {
            if (request.yAxisId->has_value() &&
                std::ranges::none_of(edited.axes, [&](const auto &axis) { return axis.id == **request.yAxisId; }))
                return {{}, SeriesMutationFailureCode::UnknownAxisId};
            config.yAxisId = *request.yAxisId;
        }
        return install(document, std::move(edited));
    }

    MutationResult removeSeries(RunSeriesDocument &document, const SeriesId id) {
        const auto index = indexOf(document, id);
        if (!index) return {{}, SeriesMutationFailureCode::UnknownSeriesId};
        auto edited = document;
        edited.series.erase(edited.series.begin() + static_cast<std::ptrdiff_t>(*index));
        normalizeDisplayPositions(edited.series);
        return install(document, std::move(edited));
    }

    MutationResult reorderSeries(RunSeriesDocument &document, const SeriesId id, const uint32_t position) {
        const auto index = indexOf(document, id);
        if (!index) return {{}, SeriesMutationFailureCode::UnknownSeriesId};
        if (position >= document.series.size()) return {{}, SeriesMutationFailureCode::DisplayPositionOutOfRange};
        if (*index == position) return {};
        auto edited = document;
        auto record = std::move(edited.series[*index]);
        edited.series.erase(edited.series.begin() + static_cast<std::ptrdiff_t>(*index));
        edited.series.insert(edited.series.begin() + position, std::move(record));
        normalizeDisplayPositions(edited.series);
        return install(document, std::move(edited));
    }

    MutationResult createAxis(RunSeriesDocument &document, const CreateAxisRequest &request) {
        if (!document.nextAxisId) return {{}, SeriesMutationFailureCode::AxisIdExhausted};
        auto edited = document;
        const auto id = *edited.nextAxisId;
        edited.axes.push_back(AxisConfig{id, request.name, {}, AxisTransformKind::Identity});
        edited.nextAxisId = successor(id);
        return install(document, std::move(edited), std::nullopt, id);
    }

    MutationResult deleteAxis(RunSeriesDocument &document, const AxisId id) {
        const auto found = std::ranges::find(document.axes, id, &AxisConfig::id);
        if (found == document.axes.end()) return {{}, SeriesMutationFailureCode::UnknownAxisId};
        auto edited = document;
        edited.axes.erase(edited.axes.begin() + std::distance(document.axes.begin(), found));
        for (auto &config: edited.series) if (config.yAxisId == id) config.yAxisId = std::nullopt;
        return install(document, std::move(edited));
    }
}
