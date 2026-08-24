#pragma once

#include <QMutex>

#include <tuple>

#include "data/interfaces/i_series_config_store.h"
#include "data/interfaces/i_settings_service.h"

namespace ksv::qt_data {
    class SeriesConfigStore final : public application::ISeriesConfigStore {
    public:
        explicit SeriesConfigStore(std::shared_ptr<application::ISettingsService> settingsService);

        [[nodiscard]] std::vector<application::SeriesConfig> getAll() const override;
        application::MutationResult createComputed(const application::CreateComputedSeriesRequest &) override;
        application::MutationResult updateSeries(const application::UpdateSeriesRequest &) override;
        application::MutationResult removeComputed(application::SeriesId) override;
        application::MutationResult reorder(application::SeriesId, uint32_t position) override;
        void onChanged(std::function<void()> callback) override;

        [[nodiscard]] std::vector<application::AxisConfig> getAllAxes() const override;
        application::MutationResult createAxis(const application::CreateAxisRequest &) override;
        application::MutationResult deleteAxis(application::AxisId) override;

        void beginDraft() override;
        application::MutationResult commitDraft() override;
        void discardDraft() override;
        [[nodiscard]] bool hasPendingChanges() const override;

    private:
        void ensureLoadedLocked() const;
        void seedLocked(const std::string *invalidRaw = nullptr) const;
        void writeLocked(const std::vector<application::SeriesConfig> &, const application::SeriesId &next,
                         const std::vector<application::AxisConfig> &, const application::AxisId &nextAxisId) const;
        application::MutationResult commitLocked(std::vector<application::SeriesConfig>,
                                                 application::SeriesId next,
                                                 std::vector<application::AxisConfig>,
                                                 application::AxisId nextAxisId,
                                                 std::optional<application::SeriesId> createdSeriesId = std::nullopt,
                                                 std::optional<application::AxisId> createdAxisId = std::nullopt);
        [[nodiscard]] std::optional<size_t> indexOfLocked(application::SeriesId) const;
        application::MutationResult mutateLocked(
            const std::function<application::MutationResult(std::vector<application::SeriesConfig> &)> &mutate);

        std::shared_ptr<application::ISettingsService> m_settingsService;
        // Guards this store's cache; SettingsService synchronizes its backing QSettings instance.
        mutable QMutex m_mutex;
        mutable std::vector<application::SeriesConfig> m_configs;
        mutable std::optional<application::SeriesId> m_next;
        mutable std::vector<application::AxisConfig> m_axes;
        mutable std::optional<application::AxisId> m_nextAxisId;
        mutable bool m_requiresReload = true;
        std::vector<std::function<void()>> m_callbacks;
        mutable std::optional<std::tuple<std::vector<application::SeriesConfig>, application::SeriesId,
                                         std::vector<application::AxisConfig>, application::AxisId>> m_draftBaseline;
        mutable bool m_draftActive = false;
    };
}
