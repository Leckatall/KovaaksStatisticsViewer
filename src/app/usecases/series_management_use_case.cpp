#include "series_management_use_case.h"

#include <utility>

namespace ksv::application {
    namespace {
        MutationResult rejected() {
            MutationResult result;
            result.failure = SeriesMutationFailureCode::ConfigurationRejected;
            return result;
        }
    }

    SeriesManagementUseCase::SeriesManagementUseCase(std::shared_ptr<IGraphConfigurationUseCase> configuration)
        : m_configuration(std::move(configuration)) {
        m_configuration->onChanged([this](const GraphConfigurationChange &change) {
            if (change.runSeries) notify();
        });
    }

    RunSeriesDocument SeriesManagementUseCase::committedRunSeries() const {
        return m_configuration->current().configuration->runSeries;
    }

    void SeriesManagementUseCase::notify() const {
        for (const auto &callback: m_callbacks) callback();
    }

    std::vector<SeriesConfig> SeriesManagementUseCase::getAll() const {
        return m_draft ? m_draft->series : committedRunSeries().series;
    }

    std::vector<AxisConfig> SeriesManagementUseCase::getAllAxes() const {
        return m_draft ? m_draft->axes : committedRunSeries().axes;
    }

    void SeriesManagementUseCase::onChanged(std::function<void()> callback) {
        m_callbacks.push_back(std::move(callback));
    }

    MutationResult SeriesManagementUseCase::edit(const std::function<MutationResult(RunSeriesDocument &)> &mutate) {
        if (m_draft) {
            auto result = mutate(*m_draft);
            if (result.succeeded()) notify();
            return result;
        }
        auto candidate = *m_configuration->current().configuration;
        auto result = mutate(candidate.runSeries);
        if (!result.succeeded()) return result;
        if (!m_configuration->save(candidate).accepted()) return rejected();
        return result;
    }

    MutationResult SeriesManagementUseCase::setSeriesEnabled(const SeriesId id, const bool enabled) {
        return updateSeries({.id = id, .presentation = UpdatedSeriesPresentation{.enabled = enabled}});
    }

    MutationResult SeriesManagementUseCase::createComputed(const CreateComputedSeriesRequest &request) {
        return edit([&](RunSeriesDocument &document) { return createComputedSeries(document, request); });
    }

    MutationResult SeriesManagementUseCase::updateSeries(const UpdateSeriesRequest &request) {
        return edit([&](RunSeriesDocument &document) { return application::updateSeries(document, request); });
    }

    MutationResult SeriesManagementUseCase::removeComputed(const SeriesId id) {
        return edit([&](RunSeriesDocument &document) { return removeSeries(document, id); });
    }

    MutationResult SeriesManagementUseCase::reorder(const SeriesId id, const uint32_t position) {
        return edit([&](RunSeriesDocument &document) { return reorderSeries(document, id, position); });
    }

    MutationResult SeriesManagementUseCase::createAxis(const CreateAxisRequest &request) {
        return edit([&](RunSeriesDocument &document) { return application::createAxis(document, request); });
    }

    MutationResult SeriesManagementUseCase::deleteAxis(const AxisId id) {
        return edit([&](RunSeriesDocument &document) { return application::deleteAxis(document, id); });
    }

    void SeriesManagementUseCase::beginDraft() {
        if (hasPendingChanges()) return;
        m_draftBaseline = committedRunSeries();
        m_draft = m_draftBaseline;
    }

    MutationResult SeriesManagementUseCase::commitDraft() {
        if (!m_draft) return {};
        // Only the Graph Lines section is drafted here, so it joins the latest committed preferences.
        auto candidate = *m_configuration->current().configuration;
        candidate.runSeries = *m_draft;
        // Rebaselined first so observers of the resulting publication already see a clean draft.
        auto previousBaseline = std::exchange(m_draftBaseline, *m_draft);
        if (m_configuration->save(candidate).accepted()) return {};
        m_draftBaseline = std::move(previousBaseline);
        return rejected();
    }

    void SeriesManagementUseCase::discardDraft() {
        if (!m_draft) return;
        m_draftBaseline = committedRunSeries();
        m_draft = m_draftBaseline;
        notify();
    }

    bool SeriesManagementUseCase::hasPendingChanges() const {
        return m_draft && !(*m_draft == m_draftBaseline);
    }
}
