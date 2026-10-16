#include "series_config_store.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <tuple>

#include "graph_configuration_codec.h"
#include "series_config_codec.h"

namespace ksv::qt_data {
    namespace {
        using namespace application;

        QString encode(const std::vector<SeriesConfig> &configs, const std::optional<SeriesId> next,
                       const std::vector<AxisConfig> &axes, const std::optional<AxisId> nextAxisId) {
            auto root = series_codec::encodeRunSeries({configs, next, axes, nextAxisId});
            root["schemaVersion"] = 3;
            return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
        }

        void normalizeDisplayPositions(std::vector<SeriesConfig> &configs) {
            for (size_t index = 0; index < configs.size(); ++index)
                configs[index].presentation.displayPosition = static_cast<uint32_t>(index);
        }
    }

    SeriesConfigStore::SeriesConfigStore(std::shared_ptr<application::ISettingsService> settingsService)
        : m_settingsService(std::move(settingsService)) {
        QMutexLocker lock(&m_mutex);
        ensureLoadedLocked();
    }

    void SeriesConfigStore::writeLocked(const std::vector<SeriesConfig> &configs, const std::optional<SeriesId> next,
                                        const std::vector<AxisConfig> &axes, const std::optional<AxisId> nextAxisId) const {
        m_settingsService->setSeriesConfigDocument(encode(configs, next, axes, nextAxisId).toStdString());
    }

    void SeriesConfigStore::seedLocked(const std::string *invalidRaw) const {
        qDebug() << "Seeding series config store";
        if (invalidRaw) m_settingsService->quarantineSeriesConfigDocument(*invalidRaw);
        auto [configs, next, axes, nextAxisId] =
                series_codec::seededRunSeries(m_settingsService->getLegacyDisabledColumnKeys());
        m_configs = std::move(configs);
        m_next = next;
        m_axes = std::move(axes);
        m_nextAxisId = nextAxisId;
        writeLocked(m_configs, m_next, m_axes, m_nextAxisId);
        m_requiresReload = false;
    }

    void SeriesConfigStore::ensureLoadedLocked() const {
        if (!m_requiresReload) return;
        if (!m_settingsService->hasSeriesConfigDocument()) {
            seedLocked();
            return;
        }
        const auto raw = m_settingsService->getSeriesConfigDocument();
        // Graph preferences in a version-4 document are not this store's concern; only its series section is.
        const auto decoded = decodeGraphConfigurationDocument(raw);
        if (!decoded || decoded->runSeriesRejected) {
            seedLocked(&raw);
            return;
        }
        const auto &runSeries = decoded->configuration.runSeries;
        m_configs = runSeries.series;
        m_next = runSeries.nextSeriesId;
        m_axes = runSeries.axes;
        m_nextAxisId = runSeries.nextAxisId;
        m_requiresReload = false;
    }

    std::vector<SeriesConfig> SeriesConfigStore::getAll() const {
        QMutexLocker lock(&m_mutex);
        ensureLoadedLocked();
        return m_configs;
    }

    std::vector<AxisConfig> SeriesConfigStore::getAllAxes() const {
        QMutexLocker lock(&m_mutex);
        ensureLoadedLocked();
        return m_axes;
    }

    void SeriesConfigStore::onChanged(std::function<void()> callback) {
        QMutexLocker lock(&m_mutex);
        m_callbacks.push_back(std::move(callback));
    }

    std::optional<size_t> SeriesConfigStore::indexOfLocked(const SeriesId series_id) const {
        const auto index = std::ranges::find_if(m_configs, [&](const auto &config) { return config.id == series_id; });
        if (index == m_configs.end()) {
            qCritical() << "Requested series id " << series_id.value << " not found in series config store";
            return {};
        };
        return std::distance(m_configs.begin(), index);
    }

    MutationResult SeriesConfigStore::commitLocked(std::vector<SeriesConfig> configs, std::optional<SeriesId> next,
                                                   std::vector<AxisConfig> axes, std::optional<AxisId> nextAxisId,
                                                   std::optional<SeriesId> createdSeriesId,
                                                   std::optional<AxisId> createdAxisId) {
        const auto errors = validateSeriesConfigs(configs);
        if (!errors.empty()) return {errors};
        if (!m_draftActive) writeLocked(configs, next, axes, nextAxisId);
        m_configs = std::move(configs);
        m_next = next;
        m_axes = std::move(axes);
        m_nextAxisId = nextAxisId;
        m_requiresReload = false;
        return {{}, {}, false, createdSeriesId, createdAxisId};
    }

    void SeriesConfigStore::beginDraft() {
        QMutexLocker lock(&m_mutex);
        ensureLoadedLocked();
        m_draftBaseline = {m_configs, m_next, m_axes, m_nextAxisId};
        m_draftActive = true;
    }

    MutationResult SeriesConfigStore::commitDraft() {
        QMutexLocker lock(&m_mutex);
        if (!m_draftActive) return {};
        writeLocked(m_configs, m_next, m_axes, m_nextAxisId);
        m_draftBaseline.reset();
        m_draftActive = false;
        return {};
    }

    void SeriesConfigStore::discardDraft() {
        std::vector<std::function<void()>> callbacks; {
            QMutexLocker lock(&m_mutex);
            if (!m_draftActive) return;
            std::tie(m_configs, m_next, m_axes, m_nextAxisId) = *m_draftBaseline;
            m_draftBaseline.reset();
            m_draftActive = false;
            callbacks = m_callbacks;
        }
        for (auto &callback: callbacks) callback();
    }

    bool SeriesConfigStore::hasPendingChanges() const {
        QMutexLocker lock(&m_mutex);
        if (!m_draftActive) return false;
        const auto &[configs, next, axes, nextAxisId] = *m_draftBaseline;
        return !sameSeriesConfigs(m_configs, configs) || m_next != next || !sameAxisConfigs(m_axes, axes) ||
               m_nextAxisId != nextAxisId;
    }

    MutationResult SeriesConfigStore::mutateLocked(
        const std::function<MutationResult(std::vector<SeriesConfig> &, std::optional<SeriesId> &,
                                            std::vector<AxisConfig> &, std::optional<AxisId> &)> &mutate) {
        std::vector<std::function<void()> > callbacks;
        MutationResult result; {
            QMutexLocker lock(&m_mutex);
            ensureLoadedLocked();
            auto configs = m_configs;
            auto next = m_next;
            auto axes = m_axes;
            auto nextAxisId = m_nextAxisId;
            result = mutate(configs, next, axes, nextAxisId);
            if (result.succeeded()) callbacks = m_callbacks;
        }
        for (auto &callback: callbacks) callback();
        return result;
    }

    MutationResult SeriesConfigStore::createComputed(const CreateComputedSeriesRequest &request) {
        return mutateLocked([&](auto &configs, auto &next, auto &axes, auto &nextAxisId) -> MutationResult {
            if (!next) return {{}, StoreMutationFailureCode::ComputedSeriesIdExhausted};
            const auto id = *next;
            configs.emplace_back(SeriesConfig{
                id,
                {
                    request.presentation.name, request.presentation.lineStyle, request.presentation.enabled,
                    static_cast<uint32_t>(configs.size())
                },
                request.expression
            });
            next = id.value == UINT64_MAX ? std::nullopt : std::optional{SeriesId{id.value + 1}};
            return commitLocked(std::move(configs), next, std::move(axes), nextAxisId, id);
        });
    }

    application::MutationResult SeriesConfigStore::updateSeries(const application::UpdateSeriesRequest &request) {
        return mutateLocked([&](auto &configs, auto &next, auto &axes, auto &nextAxisId) -> MutationResult {
            const auto index = indexOfLocked(request.id);
            if (!index) return {{}, StoreMutationFailureCode::UnknownSeriesId};
            auto &config = configs[*index];
            if (request.presentation) {
                auto [name, lineStyle, enabled] = request.presentation.value();
                if (name) config.presentation.name = name.value();
                if (lineStyle) config.presentation.lineStyle = lineStyle.value();
                if (enabled) config.presentation.enabled = enabled.value();
            }
            if (request.expression) config.expression = request.expression.value();
            if (request.yAxisId) {
                if (request.yAxisId->has_value() &&
                    std::ranges::none_of(axes, [&](const auto &axis) { return axis.id == request.yAxisId->value(); }))
                    return {{}, StoreMutationFailureCode::UnknownAxisId};
                config.yAxisId = *request.yAxisId;
            }
            return commitLocked(std::move(configs), next, std::move(axes), nextAxisId);
        });
    }

    MutationResult SeriesConfigStore::removeComputed(const SeriesId id) {
        return mutateLocked([&](auto &configs, auto &next, auto &axes, auto &nextAxisId) -> MutationResult {
            const auto index = indexOfLocked(id);
            if (!index) return {{}, StoreMutationFailureCode::UnknownSeriesId};
            configs.erase(configs.begin() + *index);
            normalizeDisplayPositions(configs);
            return commitLocked(std::move(configs), next, std::move(axes), nextAxisId);
        });
    }

    MutationResult SeriesConfigStore::reorder(SeriesId reference,
                                              const uint32_t position) {
        return mutateLocked([&](auto &configs, auto &next, auto &axes, auto &nextAxisId) -> MutationResult {
            const auto index = indexOfLocked(reference);
            if (!index) return {{}, StoreMutationFailureCode::UnknownSeriesId};
            if (position >= configs.size()) return {{}, StoreMutationFailureCode::DisplayPositionOutOfRange};
            if (*index == position) return {};
            const auto record = std::move(configs[*index]);
            configs.erase(configs.begin() + *index);
            configs.insert(configs.begin() + position, record);
            normalizeDisplayPositions(configs);
            return commitLocked(std::move(configs), next, std::move(axes), nextAxisId);
        });
    }

    MutationResult SeriesConfigStore::createAxis(const CreateAxisRequest &request) {
        return mutateLocked([&](auto &configs, auto &next, auto &axes, auto &nextAxisId) -> MutationResult {
            if (!nextAxisId) return {{}, StoreMutationFailureCode::AxisIdExhausted};
            const auto id = *nextAxisId;
            axes.push_back(AxisConfig{id, request.name, {}, AxisTransformKind::Identity});
            nextAxisId = id.value == UINT64_MAX ? std::nullopt : std::optional{AxisId{id.value + 1}};
            return commitLocked(std::move(configs), next, std::move(axes), nextAxisId, std::nullopt, id);
        });
    }

    MutationResult SeriesConfigStore::deleteAxis(const AxisId id) {
        return mutateLocked([&](auto &configs, auto &next, auto &axes, auto &nextAxisId) -> MutationResult {
            const auto index = std::ranges::find_if(axes, [&](const auto &axis) { return axis.id == id; });
            if (index == axes.end()) return {{}, StoreMutationFailureCode::UnknownAxisId};
            axes.erase(index);
            for (auto &config: configs) if (config.yAxisId == id) config.yAxisId = std::nullopt;
            return commitLocked(std::move(configs), next, std::move(axes), nextAxisId);
        });
    }
}
