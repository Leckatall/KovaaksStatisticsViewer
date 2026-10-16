#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "contracts/i_graph_configuration_use_case.h"
#include "contracts/i_series_management_use_case.h"

namespace ksv::application {
    // TODO(2026-10-16): Remove the draft members once SettingsViewModel owns the whole-configuration
    // working copy (graph-configuration U3) — replaced by pure run_series_editing transformations of it.
    // Until then this holds the Graph Lines draft between beginDraft() and Save/Discard. It is never
    // accepted state: the dashboard reads IGraphConfigurationUseCase, and the draft reaches it only
    // through commitDraft()'s whole-candidate save.
    class SeriesManagementUseCase final : public ISeriesManagementUseCase {
    public:
        explicit SeriesManagementUseCase(std::shared_ptr<IGraphConfigurationUseCase> configuration);

        [[nodiscard]] std::vector<SeriesConfig> getAll() const override;
        MutationResult setSeriesEnabled(SeriesId id, bool enabled) override;
        MutationResult createComputed(const CreateComputedSeriesRequest &request) override;
        MutationResult updateSeries(const UpdateSeriesRequest &request) override;
        MutationResult removeComputed(SeriesId id) override;
        MutationResult reorder(SeriesId id, uint32_t position) override;
        [[nodiscard]] std::vector<AxisConfig> getAllAxes() const override;
        MutationResult createAxis(const CreateAxisRequest &request) override;
        MutationResult deleteAxis(AxisId id) override;
        void onChanged(std::function<void()> callback) override;

        // A visit that already holds unsaved edits keeps them; otherwise the draft is reseeded.
        void beginDraft() override;
        // On acceptance the draft stays open with the committed document as its new baseline.
        MutationResult commitDraft() override;
        void discardDraft() override;
        [[nodiscard]] bool hasPendingChanges() const override;

    private:
        // Edits the draft when one is open; otherwise saves the edited committed document at once.
        MutationResult edit(const std::function<MutationResult(RunSeriesDocument &)> &mutate);

        [[nodiscard]] RunSeriesDocument committedRunSeries() const;

        void notify() const;

        std::shared_ptr<IGraphConfigurationUseCase> m_configuration;
        std::optional<RunSeriesDocument> m_draft;
        RunSeriesDocument m_draftBaseline;
        std::vector<std::function<void()>> m_callbacks;
    };
}
