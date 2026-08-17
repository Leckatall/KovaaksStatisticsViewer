#pragma once

#include <QMutex>

#include "data/interfaces/i_series_config_store.h"
#include "series_config_store_settings_backend.h"

namespace ksv::qt_data {
    class SeriesConfigStore final : public application::ISeriesConfigStore {
    public:
        explicit SeriesConfigStore(std::shared_ptr<ISeriesConfigStoreSettingsBackend> backend);
        explicit SeriesConfigStore(QSettings::Format format = QSettings::NativeFormat);

        [[nodiscard]] std::vector<application::SeriesConfig> getAll() const override;
        application::MutationResult createComputed(const application::CreateComputedSeriesRequest &) override;
        application::MutationResult updateComputed(const application::UpdateComputedSeriesRequest &) override;
        application::MutationResult updateBase(const application::UpdateBaseSeriesRequest &) override;
        application::MutationResult removeComputed(application::ComputedSeriesId) override;
        application::MutationResult reorder(application::SeriesRecordReference, uint32_t position) override;
        void onChanged(std::function<void()> callback) override;

    private:
        void ensureLoadedLocked() const;
        void seedLocked(const QVariant *invalidRaw = nullptr) const;
        bool writeLocked(const std::vector<application::SeriesConfig> &, const std::optional<application::ComputedSeriesId> &) const;
        application::MutationResult commitLocked(std::vector<application::SeriesConfig>, std::optional<application::ComputedSeriesId>,
                                                 std::optional<application::ComputedSeriesId> created = std::nullopt);
        [[nodiscard]] std::optional<size_t> indexOfLocked(application::SeriesRecordReference) const;
        application::MutationResult mutateLocked(
            const std::function<application::MutationResult(std::vector<application::SeriesConfig> &)> &mutate);

        std::shared_ptr<ISeriesConfigStoreSettingsBackend> m_backend;
        mutable QMutex m_mutex;
        mutable std::vector<application::SeriesConfig> m_configs;
        mutable std::optional<application::ComputedSeriesId> m_next;
        mutable bool m_requiresReload = true;
        std::vector<std::function<void()>> m_callbacks;
    };
}
